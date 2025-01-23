#include <actions/minifyAction.hpp>
#include <format/minifyFormat.hpp>
#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory options("options");
static cl::opt<std::string> file(
    cl::Positional,
    cl::desc("<source>"),
    cl::cat(options));
static cl::opt<bool> inPlace(
    "i",
    cl::desc("Whether to process the file in place"),
    cl::value_desc("inplace"), cl::init(false), cl::cat(options));
static cl::list<std::string> ArgsAfter(
    "extra-arg",
    cl::desc("Additional argument to append to the compiler command line"),
    cl::sub(cl::SubCommand::getAll()), cl::cat(options));

FileID createInMemoryFile(StringRef FileName, MemoryBufferRef Source,
                          SourceManager &Sources, FileManager &Files,
                          llvm::vfs::InMemoryFileSystem *MemFS)
{
    MemFS->addFileNoOwn(FileName, 0, Source);
    auto File = Files.getOptionalFileRef(FileName);
    assert(File && "File not added to MemFS?");
    return Sources.createFileID(*File, SourceLocation(), SrcMgr::C_User);
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
        "If -i is specified, the file is edited in-place. Otherwise, the\n"
        "result is written to the standard output.\n");

    // read in file
    string fileName = file.getValue();
    unique_ptr<MemoryBuffer> code;
    if (fileName.empty())
    {
        errs() << "Please provide a source file. Or, use the --help flag for information on how to use this tool.\n";
        return 1;
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

    IntrusiveRefCntPtr<llvm::vfs::FileSystem> fs = llvm::vfs::getRealFileSystem();
    // IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> fs = IntrusiveRefCntPtr(new llvm::vfs::InMemoryFileSystem);
    IntrusiveRefCntPtr<FileManager> fileManagerPtr(new FileManager(FileSystemOptions(), fs));
    IntrusiveRefCntPtr<DiagnosticOptions> diagOpts(new DiagnosticOptions());
    DiagnosticsEngine diagnostics(
        IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs), &*diagOpts);
    SourceManager sm(diagnostics, *fileManagerPtr);
    // FileID id = createInMemoryFile(fileName, *code, sm, *fileManagerPtr, fs.get());
    // sm.setMainFileID(id);

    // apply minify action
    if (compDB == nullptr)
    {
        errs() << "Please provide compilation options with -- \n";
        return 3;
    }
    Replacements replacements;
    ClangTool tool(*compDB, {fileName}, make_shared<PCHContainerOperations>(), fs);
    if (tool.run(MinifierAction::newMinifierAction(&replacements).get()))
    {
        // error while running the tool
        return 4;
    }

    // apply those rewrites
    sm.setMainFileID(sm.getOrCreateFileID(*fileManagerPtr->getFileRef(fileName), SrcMgr::C_User));
    Rewriter rewriter(sm, LangOptions());
    if (!applyAllReplacements(replacements, rewriter))
    {
        errs() << "Failed to apply minify action rewrites!\n";
        return 5;
    }

    // format
    MinifyFormatter formatter(sm);
    replacements = formatter.process();

    // save format replacements too
    if (!applyAllReplacements(replacements, rewriter))
    {
        llvm::errs() << "Failed to apply minify format rewrites\n";
        return 6;
    }

    // output
    RewriteBuffer &buffer = rewriter.getEditBuffer(sm.getMainFileID());
    if (inPlace.getValue())
    {
        error_code ec;
        raw_fd_ostream out(fileName, ec);
        buffer.write(out);
        out.close();
    }
    else
    {
        buffer.write(outs());
    }
}