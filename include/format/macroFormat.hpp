#pragma once
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Format/Format.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>

/**
 * @brief Adds macro defines to the top of the file
 * to minimize repeated token sequences
 *
 */
class MacroFormatter
{
public:
    MacroFormatter(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> fileSystem, const std::string &mainFileName, int firstUnusedSymbol);
    clang::tooling::Replacements process();

private:
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> fileSystem;
    const std::string &mainFileName;
    int firstUnusedSymbol;
};