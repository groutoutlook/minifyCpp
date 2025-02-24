#include <actions/AddDefinesAction.hpp>
#include <actions/ExpandMacroAction.hpp>
#include <actions/FormatAction.hpp>
#include <actions/MinifySymbolsAction.hpp>
#include <actions/PPSymbolsAction.hpp>
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

// arguments
static cl::OptionCategory options("Minifier Options");
static cl::opt<std::string> file(
    cl::Positional,
    cl::desc("[source]"),
    cl::cat(options));
static cl::opt<bool> inPlace(
    "i",
    cl::desc("Whether to process the file in place, only works if [source] is specified"),
    cl::value_desc("inplace"), cl::init(false), cl::cat(options));
static cl::opt<bool> expandAll(
    "expand-all",
    cl::desc("Whether to expand all macros encountered in the source file"),
    cl::value_desc("expand-all"), cl::init(false), cl::cat(options));
static cl::opt<bool> noAddMacros(
    "no-add-macros",
    cl::desc("Disable minimizing the file by finding repeated subsequences and defining those as body macros"),
    cl::value_desc("no-add-macros"), cl::init(false), cl::cat(options));
static cl::opt<bool> noNiceMacros(
    "no-nice-macros",
    cl::desc("Disable only adding body macros that have matched open/close parentheses/brackets/braces"),
    cl::value_desc("no-nice-macros"), cl::init(false), cl::cat(options));
static cl::list<std::string> argsAfter(
    "extra-arg",
    cl::desc("Additional argument to append to the compiler command line"),
    cl::sub(cl::SubCommand::getAll()), cl::cat(options));

// helper function to simplify writing to a file
bool writeToFile(const StringRef fileName, const StringRef code)
{
    error_code ec;
    raw_fd_ostream out(fileName, ec);
    out << code;
    out.close();
    return !ec;
}

// helper function to create a clang tool
ClangTool createTool(CompilationDatabase *compDB, string mainFileName, IntrusiveRefCntPtr<vfs::FileSystem> vfs)
{
    // create a clang tool
    return ClangTool(*compDB, {mainFileName}, make_shared<PCHContainerOperations>(), vfs);
}

/**
 * @brief Updates the main file's contents
 *
 * @param sm
 * @param replacements
 * @return true on success
 * @return false on failure
 */
bool updateMainFileContents(IntrusiveRefCntPtr<vfs::OverlayFileSystem> vfs, const string &mainFileName, Replacements &replacements)
{
    StringRef mainFileContents = vfs->getBufferForFile(mainFileName)->get()->getBuffer();
    Expected<string> mainFileContentsAfterReplacements = applyAllReplacements(mainFileContents, replacements);
    if (!mainFileContentsAfterReplacements)
    {
        return false;
    }

    // update the file in the overlay
    // since there's no direct way to edit, simply add a new layer on top to override the contents
    IntrusiveRefCntPtr<vfs::InMemoryFileSystem> topLayer = new vfs::InMemoryFileSystem();
    topLayer->addFile(mainFileName, 0, MemoryBuffer::getMemBufferCopy(*mainFileContentsAfterReplacements));
    vfs->pushOverlay(topLayer);
    return true;
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

    // create FS and set up file
    const string tmpFileName = "/tmp/golfC-Minifier.c";
    IntrusiveRefCntPtr<llvm::vfs::OverlayFileSystem> overlayFS = new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem());
    IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> inMemoryFileSystem = new llvm::vfs::InMemoryFileSystem();
    overlayFS->pushOverlay(inMemoryFileSystem);
    inMemoryFileSystem->addFile(tmpFileName, 0, MemoryBuffer::getMemBufferCopy(code->getBuffer()));

    // initialize tool
    if (compDB == nullptr)
    {
        errs() << "Please provide compilation options with -- \n";
        return 4;
    }
    Replacements replacements;

    // first, get existing preprocessor defines
    set<string> definitions;
    createTool(compDB.get(), tmpFileName, overlayFS).run(PPSymbolsAction::newPPSymbolsAction(&definitions).get());

    // next up, expand macros and save the results
    if (expandAll.getValue())
    {
        createTool(compDB.get(), tmpFileName, overlayFS).run(ExpandMacroAction::newExpandMacroAction(&replacements).get());
        if (!updateMainFileContents(overlayFS, tmpFileName, replacements))
        {
            errs() << "Failed to apply expand macros action\n";
            return 5;
        }
    }

    // then run the variable minify tool
    replacements = Replacements();
    int firstUnusedSymbol = 0;
    createTool(compDB.get(), tmpFileName, overlayFS).run(MinifySymbolsAction::newMinifierAction(&replacements, &definitions, &firstUnusedSymbol).get());
    // apply those rewrites
    if (!updateMainFileContents(overlayFS, tmpFileName, replacements))
    {
        errs() << "Failed to apply minify action rewrites!\n";
        return 6;
    }

    // combine / add macros
    if (!noAddMacros.getValue())
    {
        replacements = Replacements();
        createTool(compDB.get(), tmpFileName, overlayFS).run(AddDefinesAction::newAddDefinesAction(firstUnusedSymbol, !noNiceMacros.getValue(), &replacements).get());
        // apply the rewrites
        if (!updateMainFileContents(overlayFS, tmpFileName, replacements))
        {
            errs() << "Failed to apply macro format rewrites!\n";
            return 7;
        }
    }
    // minify format (remove spaces)
    replacements = Replacements();
    createTool(compDB.get(), tmpFileName, overlayFS).run(FormatAction::newFormatAction(&replacements).get());
    // save format replacements too
    if (!updateMainFileContents(overlayFS, tmpFileName, replacements))
    {
        llvm::errs() << "Failed to apply minify format rewrites\n";
        return 8;
    }

    // output.
    StringRef finalOutput = overlayFS->getBufferForFile(tmpFileName)->get()->getBuffer();
    if (inPlace.getValue() && !fromSTDIN)
    {
        writeToFile(fileName, finalOutput);
    }
    else
    {
        outs() << finalOutput;
    }
}