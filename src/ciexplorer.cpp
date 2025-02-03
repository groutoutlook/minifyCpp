#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Lex/MacroArgs.h>
#include <llvm/Support/CommandLine.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

class IncludePPCallbacks : public PPCallbacks
{
private:
    SourceManager &manager;

public:
    IncludePPCallbacks(SourceManager &manager) : manager(manager) {};
    virtual void InclusionDirective(SourceLocation HashLoc,
                                    const Token &IncludeTok, StringRef FileName,
                                    bool IsAngled, CharSourceRange FilenameRange,
                                    OptionalFileEntryRef File,
                                    StringRef SearchPath, StringRef RelativePath,
                                    const Module *Imported,
                                    SrcMgr::CharacteristicKind FileType)
    {
        if (manager.isWrittenInMainFile(FilenameRange.getAsRange().getBegin()))
        {
            outs() << "got an include for " << FileName << " isangled: " << IsAngled << "\n";
        }
    }
};

void process(SourceManager &sm, Preprocessor &preproc)
{
    // create a copy of the preprocessor
    // LangOptions langOpts(originalPreproc.getLangOpts());
    // shared_ptr<HeaderSearchOptions> hSearchOptions = make_shared<HeaderSearchOptions>(originalPreproc.getHeaderSearchInfo().getHeaderSearchOpts());
    // HeaderSearch hSearch(hSearchOptions, originalPreproc.getSourceManager(), originalPreproc.getDiagnostics(), langOpts, &originalPreproc.getTargetInfo());
    // shared_ptr<PreprocessorOptions> procOptions = make_shared<PreprocessorOptions>(originalPreproc.getPreprocessorOpts());
    // Preprocessor preproc(procOptions, originalPreproc.getDiagnostics(), langOpts, originalPreproc.getSourceManager(), hSearch, originalPreproc.getModuleLoader());
    // preproc.Initialize(originalPreproc.getTargetInfo(), originalPreproc.getAuxTargetInfo());
    // ApplyHeaderSearchOptions(hSearch, *hSearchOptions,
    //                          langOpts, originalPreproc.getTargetInfo().getTriple());
    // outs() << originalPreproc.getTargetInfo().getTriple().str() << "\n";

    // main stuff
    preproc.addPPCallbacks(make_unique<IncludePPCallbacks>(sm));
    preproc.EnterMainSourceFile();
    Token tok;
    preproc.Lex(tok);
    SourceLocation prevLocation = sm.getLocForStartOfFile(sm.getMainFileID());

    while (!tok.is(tok::eof))
    {
        SourceLocation fileLoc = sm.getFileLoc(tok.getLocation());
        if (sm.getFileID(fileLoc) == sm.getMainFileID())
        {
            outs() << "got token " << tok.getName() << " " << preproc.getSpelling(tok) << "\n";
        }
        preproc.Lex(tok);
    }
    preproc.PrintStats();
}

class ExplorerAction : public clang::PreprocessOnlyAction
{
public:
    virtual void ExecuteAction() override
    {
        CompilerInstance &compiler = getCompilerInstance();
        process(compiler.getSourceManager(), compiler.getPreprocessor());
    }
};

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

int main(int argc, const char **argv)
{
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
    if (!ExpectedParser)
    {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());
    for (const auto &source : OptionsParser.getSourcePathList())
    {
        cout << "Examining " << source << endl;
    }

    return Tool.run(newFrontendActionFactory<ExplorerAction>().get());
}