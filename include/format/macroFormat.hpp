#pragma once
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Format/Format.h>

/**
 * @brief Adds macro defines to the top of the file
 * to minimize repeated token sequences
 *
 */
class MacroFormatter
{
public:
    MacroFormatter(clang::SourceManager &sm, int firstUnusedSymbol);
    clang::tooling::Replacements process();

private:
    clang::SourceManager &sm;
    int firstUnusedSymbol;
};