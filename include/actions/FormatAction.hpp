#pragma once
#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Core/Replacement.h>
#include <memory>

class FormatAction : public clang::PreprocessorFrontendAction
{
public:
    FormatAction(clang::tooling::Replacements *replacements);
    virtual void ExecuteAction() override;
    static std::unique_ptr<clang::tooling::FrontendActionFactory> newFormatAction(clang::tooling::Replacements *replacements);

private:
    clang::tooling::Replacements *replacements;
};
