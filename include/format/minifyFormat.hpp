#pragma once
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Format/Format.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>

class MinifyFormatter
{
public:
    MinifyFormatter(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> fileSystem, const std::string &mainFileName);
    clang::tooling::Replacements process();

private:
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> fileSystem;
    const std::string &mainFileName;
};
