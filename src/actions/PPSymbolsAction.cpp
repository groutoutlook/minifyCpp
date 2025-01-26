#include <actions/PPSymbolsAction.hpp>
#include <clang/Frontend/CompilerInstance.h>
using namespace clang;
using namespace std;
using namespace clang::tooling;

class PPSymbolCallbacks : public PPCallbacks
{
private:
    set<string> *definitions;

public:
    PPSymbolCallbacks(set<string> *definitions) : definitions(definitions) {};
    virtual void MacroDefined(const Token &macroNameTok, const MacroDirective *MD) override
    {
        StringRef name = macroNameTok.getIdentifierInfo()->getName();
        definitions->emplace(name);
    }
};

PPSymbolsAction::PPSymbolsAction(set<string> *definitions) : definitions(definitions) {};
unique_ptr<ASTConsumer> PPSymbolsAction::CreateASTConsumer(CompilerInstance &compiler, StringRef inFile)
{
    // we just need to get preprocessor symbols
    compiler.getPreprocessor().addPPCallbacks(make_unique<PPSymbolCallbacks>(definitions));
    return make_unique<ASTConsumer>(); // don't actually need anything w/ the ast
}
unique_ptr<FrontendActionFactory> PPSymbolsAction::newPPSymbolsAction(set<string> *definitions)
{
    class Adapter : public FrontendActionFactory
    {
    private:
        set<string> *definitions;

    public:
        Adapter(set<string> *definitions) : definitions(definitions) {};
        virtual unique_ptr<FrontendAction> create() override
        {
            return make_unique<PPSymbolsAction>(definitions);
        }
    };
    return make_unique<Adapter>(definitions);
}