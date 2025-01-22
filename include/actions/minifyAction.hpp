#include <clang/Frontend/FrontendAction.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <memory>
using namespace llvm;
class MinifierAction : public clang::ASTFrontendAction
{
private:
    std::unique_ptr<clang::Rewriter> rewriter;

public:
    virtual std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance &compiler,
                      llvm::StringRef inFile) override;
    virtual void EndSourceFileAction() override;
};
