#include <actions/ExpandMacroAction.hpp>
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
#include <clang/Rewrite/Core/Rewriter.h>
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
    ostringstream &ss;

public:
    IncludePPCallbacks(SourceManager &manager, ostringstream &ss) : manager(manager), ss(ss) {};
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
            if (IsAngled)
            {
                ss << "\n#include<" << FileName.str() << ">\n";
            }
            else
            {
                ss << "\n#include\"" << FileName.str() << "\"\n";
            }
        }
    }
};

void process(SourceManager &sm, Preprocessor &preproc, Replacements *r)
{
    // preparation
    ostringstream o;
    preproc.addPPCallbacks(make_unique<IncludePPCallbacks>(sm, o));
    preproc.EnterMainSourceFile();

    // main loop
    Token tok;
    preproc.Lex(tok);
    while (!tok.is(tok::eof))
    {
        SourceLocation fileLoc = sm.getFileLoc(tok.getLocation());
        if (sm.getFileID(fileLoc) == sm.getMainFileID())
        {
            o << preproc.getSpelling(tok) << " ";
        }
        preproc.Lex(tok);
    }

    // output
    SourceLocation mainFileBegin = sm.getLocForStartOfFile(sm.getMainFileID());
    SourceLocation mainFileEnd = tok.getLocation();
    const CharSourceRange &mainFileRange = CharSourceRange::getCharRange(SourceRange(mainFileBegin, mainFileEnd));
    cantFail(r->add(Replacement(sm, mainFileRange, o.str())));
}
void ExpandMacroAction::ExecuteAction()
{
    CompilerInstance &compiler = getCompilerInstance();
    process(compiler.getSourceManager(), compiler.getPreprocessor(), replacements);
}
ExpandMacroAction::ExpandMacroAction(Replacements *replacements) : replacements(replacements) {}
unique_ptr<FrontendActionFactory> ExpandMacroAction::newExpandMacroAction(Replacements *replacements)
{
    class Adapter : public FrontendActionFactory
    {
    private:
        Replacements *replacements;

    public:
        Adapter(Replacements *replacements) : replacements(replacements) {};
        virtual unique_ptr<FrontendAction> create() override
        {
            return make_unique<ExpandMacroAction>(replacements);
        }
    };
    return make_unique<Adapter>(replacements);
}