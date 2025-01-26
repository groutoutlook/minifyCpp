#pragma once
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <memory>
#include <set>
#include <string>

class PPSymbolsAction : public clang::ASTFrontendAction
{
private:
    std::set<std::string> *definitions;

public:
    PPSymbolsAction(std::set<std::string> *definitions);
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &compiler,
                                                                  llvm::StringRef inFile) override;

    /**
     * @brief
     *
     * @param definitions out, where to put all the found definitions
     * @return std::unique_ptr<clang::tooling::FrontendActionFactory>
     */
    static std::unique_ptr<clang::tooling::FrontendActionFactory> newPPSymbolsAction(std::set<std::string> *definitions);
};