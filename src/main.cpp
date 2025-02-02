#include <actions/expandMacroAction.hpp>
#include <actions/minifyAction.hpp>
#include <actions/PPSymbolsAction.hpp>
#include <format/minifyFormat.hpp>
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
static cl::list<std::string> ArgsAfter(
    "extra-arg",
    cl::desc("Additional argument to append to the compiler command line"),
    cl::sub(cl::SubCommand::getAll()), cl::cat(options));

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
            return 4;
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
    const string tempFileName = "/tmp/golfC-Minifier.c";
    error_code ignored;
    raw_fd_ostream tmpFileWriter(tempFileName, ignored);
    tmpFileWriter << code->getBuffer();
    tmpFileWriter.close();

    // set main file, create rewriter
    sm.setMainFileID(sm.getOrCreateFileID(*fileManagerPtr->getFileRef(tempFileName), SrcMgr::C_User));

    // apply minify action
    if (compDB == nullptr)
    {
        errs() << "Please provide compilation options with -- \n";
        return 3;
    }
    Replacements replacements;
    ClangTool tool(*compDB, {tempFileName}, make_shared<PCHContainerOperations>(), fs);
    // first, get existing preprocessor defines
    set<string> definitions;
    tool.run(PPSymbolsAction::newPPSymbolsAction(&definitions).get());

    // next up, expand macros and save the results
    tool.run(ExpandMacroAction::newExpandMacroAction(&replacements).get());
    Rewriter rewriter(sm, LangOptions());
    if (!applyAllReplacements(replacements, rewriter) ||
        rewriter.overwriteChangedFiles())
    {
        errs() << "Failed to apply expand action rewrites!\n";
        return 5;
    }

    errs() << "made changes right now, wait for more...\n";
    std::this_thread::sleep_for(std::chrono::seconds(15));
    errs() << "going on\n";

    // then run the minify tool
    // replacements = Replacements{};
    // if (tool.run(MinifierAction::newMinifierAction(&replacements, &definitions).get()))
    //{
    //    // error while running the tool
    //    return 4;
    //}

    // apply those rewrites
    // rewriter = Rewriter(sm, LangOptions());
    // if (!applyAllReplacements(replacements, rewriter) || rewriter.overwriteChangedFiles())
    //{
    //    errs() << "Failed to apply minify action rewrites!\n";
    //    return 5;
    //}

    // format
    MinifyFormatter formatter(sm);
    replacements = formatter.process();

    // save format replacements too
    rewriter = Rewriter(sm, LangOptions());
    if (!applyAllReplacements(replacements, rewriter) || rewriter.overwriteChangedFiles())
    {
        llvm::errs() << "Failed to apply minify format rewrites\n";
        return 6;
    }

    // output
    ErrorOr<unique_ptr<MemoryBuffer>> outputOrErr = MemoryBuffer::getFileAsStream(tempFileName);
    unique_ptr<MemoryBuffer> finalOutput;
    if (std::error_code ec = outputOrErr.getError())
    {
        errs() << "failed to get final output:" << ec.message() << "\n";
        return 4;
    }
    finalOutput = std::move(outputOrErr.get());
    if (inPlace.getValue() && !fromSTDIN)
    {
        error_code ec;
        raw_fd_ostream out(fileName, ec);
        out << finalOutput->getBuffer();
        out.close();
    }
    else
    {
        outs() << finalOutput->getBuffer();
    }
}