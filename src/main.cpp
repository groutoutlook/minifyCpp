#include <actions/expandMacroAction.hpp>
#include <actions/minifyAction.hpp>
#include <actions/PPSymbolsAction.hpp>
#include <format/minifyFormat.hpp>
#include <format/macroFormat.hpp>
#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory options("options");
static cl::opt<std::string> file(
    cl::Positional,
    cl::desc("[source]"),
    cl::cat(options));
static cl::opt<bool> inPlace(
    "i",
    cl::desc("Whether to process the file in place, only works if [source] is specified"),
    cl::value_desc("inplace"), cl::init(false), cl::cat(options));
static cl::opt<bool> expandAll(
    "ea",
    cl::desc("Whether to expand all macros encountered in the source file"),
    cl::value_desc("expandAll"), cl::init(false), cl::cat(options));
static cl::list<std::string> ArgsAfter(
    "extra-arg",
    cl::desc("Additional argument to append to the compiler command line"),
    cl::sub(cl::SubCommand::getAll()), cl::cat(options));

bool writeToFile(const StringRef fileName, const StringRef code)
{
    error_code ec;
    raw_fd_ostream out(fileName, ec);
    out << code;
    out.close();
    return !ec;
}

/**
 * @brief Updates the main file's contents
 *
 * @param sm
 * @param replacements
 * @return true on success
 * @return false on failure
 */
bool updateMainFileContents(SourceManager &sm, Replacements &replacements)
{
    Rewriter rewriter(sm, LangOptions());
    if (!applyAllReplacements(replacements, rewriter))
    {
        return false;
    }

    // convert to memory buffer
    RewriteBuffer &rewriteBuffer = rewriter.getEditBuffer(sm.getMainFileID());
    string contents(rewriteBuffer.begin(), rewriteBuffer.end());

    // write to sm, make sure to make it so that sm owns the string (getMemBufferCopy)
    const FileEntry *mainFile = sm.getFileEntryForID(sm.getMainFileID());
    sm.overrideFileContents(mainFile, MemoryBuffer::getMemBufferCopy(contents));
    return !rewriter.overwriteChangedFiles(); // TODO - figure out why null characters get added between tool usages
}

int main(int argc, const char **argv)
{
    // parse command line options
    cl::HideUnrelatedOptions(options);
    string errMsg;
    unique_ptr<CompilationDatabase> compDB = FixedCompilationDatabase::loadFromCommandLine(argc, argv, errMsg);
    cl::ParseCommandLineOptions(
        argc, argv,
        "A tool to format C code\n\n"
        "If a file is provided, the contents of the file is read and formatted.\n"
        "Otherwise, the code to format is assumed to be on stdin.\n"
        "If -i is specified, the file is edited in-place. This only works when\n"
        "an input file is specified. Otherwise, the result is written to the stdout.\n");

    // read in file
    string fileName = file.getValue();
    unique_ptr<MemoryBuffer> code;
    bool fromSTDIN = fileName.empty();
    if (fromSTDIN)
    {
        errs() << "No source file provided, assuming input is on stdin\n";
        ErrorOr<unique_ptr<MemoryBuffer>> codeOrErr = MemoryBuffer::getSTDIN();
        if (std::error_code ec = codeOrErr.getError())
        {
            errs() << "failed to read input from stdin: " << ec.message() << "\n";
            return 1;
        }
        code = std::move(codeOrErr.get());
    }
    else
    {
        // read from file
        ErrorOr<unique_ptr<MemoryBuffer>> codeOrErr = MemoryBuffer::getFileAsStream(fileName);
        if (std::error_code ec = codeOrErr.getError())
        {
            errs() << fileName << ": " << ec.message() << "\n";
            return 2;
        }
        code = std::move(codeOrErr.get());
    }

    // create FS and set up file, if needed
    IntrusiveRefCntPtr<llvm::vfs::FileSystem> fs = llvm::vfs::getRealFileSystem();
    IntrusiveRefCntPtr<FileManager> fileManagerPtr(new FileManager(FileSystemOptions(), fs));
    IntrusiveRefCntPtr<DiagnosticOptions> diagOpts(new DiagnosticOptions());
    DiagnosticsEngine diagnostics(
        IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs), &*diagOpts);
    SourceManager sm(diagnostics, *fileManagerPtr);
    if (fromSTDIN)
    {
        fileName = "/tmp/golfC-Minifier.c";
        writeToFile(fileName, code->getBuffer());
    }

    // set main file, create rewriter
    sm.setMainFileID(sm.getOrCreateFileID(*fileManagerPtr->getFileRef(fileName), SrcMgr::C_User));

    // initialize tool
    if (compDB == nullptr)
    {
        errs() << "Please provide compilation options with -- \n";
        return 3;
    }
    Replacements replacements;
    ClangTool tool(*compDB, {fileName}, make_shared<PCHContainerOperations>(), fs);

    // first, get existing preprocessor defines
    set<string> definitions;
    tool.run(PPSymbolsAction::newPPSymbolsAction(&definitions).get());

    // next up, expand macros and save the results
    if (expandAll.getValue())
    {
        tool.run(ExpandMacroAction::newExpandMacroAction(&replacements).get());
        if (!updateMainFileContents(sm, replacements))
        {
            errs() << "Failed to apply expand macros action\n";
            return 4;
        }
    }

    // then run the minify tool
    replacements = Replacements();
    int firstUnusedSymbol = 0;
    if (tool.run(MinifierAction::newMinifierAction(&replacements, &definitions, &firstUnusedSymbol).get()))
    {
        // error while running the tool
        return 5;
    }

    // apply those rewrites
    if (!updateMainFileContents(sm, replacements))
    {
        errs() << "Failed to apply minify action rewrites!\n";
        return 6;
    }

    // combine / add macros
    MacroFormatter macroFormatter(sm, firstUnusedSymbol);
    replacements = macroFormatter.process();
    // apply the rewrites
    if (!updateMainFileContents(sm, replacements))
    {
        errs() << "Failed to apply macro format rewrites!\n";
        return 7;
    }

    // minify format (remove spaces)
    MinifyFormatter formatter(sm);
    replacements = formatter.process();
    // save format replacements too
    if (!updateMainFileContents(sm, replacements))
    {
        llvm::errs() << "Failed to apply minify format rewrites\n";
        return 8;
    }

    // output
    StringRef finalOutput = sm.getBufferData(sm.getMainFileID());
    if (inPlace.getValue() && !fromSTDIN)
    {
        writeToFile(fileName, finalOutput);
    }
    else
    {
        outs() << finalOutput;
    }
}