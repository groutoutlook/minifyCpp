#pragma once
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Format/Format.h>
class MinifyFormatter
{
public:
    MinifyFormatter(clang::SourceManager &sm);
    clang::tooling::Replacements process();

private:
    clang::SourceManager &sm;
};
