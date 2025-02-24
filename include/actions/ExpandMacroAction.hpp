#pragma once
#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Core/Replacement.h>
#include <memory>

class ExpandMacroAction : public clang::PreprocessOnlyAction
{

public:
    ExpandMacroAction(clang::tooling::Replacements *replacements);
    virtual void ExecuteAction() override;
    static std::unique_ptr<clang::tooling::FrontendActionFactory> newExpandMacroAction(clang::tooling::Replacements *replacements);

private:
    clang::tooling::Replacements *replacements;
};