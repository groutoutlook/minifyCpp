#pragma once

#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Core/Replacement.h>
#include <memory>
/**
 * @brief Adds macro defines to the top of the file
 * to minimize repeated token sequences
 *
 */
class AddDefinesAction : public clang::PreprocessorFrontendAction
{
public:
    AddDefinesAction(int firstUnusedSymbol, bool niceMacros, clang::tooling::Replacements *replacements);
    virtual void ExecuteAction() override;
    static std::unique_ptr<clang::tooling::FrontendActionFactory> newAddDefinesAction(int firstUnusedSymbol, bool niceMacros, clang::tooling::Replacements *replacements);

private:
    int firstUnusedSymbol;
    bool niceMacros;
    clang::tooling::Replacements *replacements;
};