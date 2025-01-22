#include <format/minifyFormat.hpp>
#include <llvm/Support/CommandLine.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::opt<std::string> file(
    "file",
    cl::desc("The file to process"),
    cl::value_desc("filename"), cl::init(""));

static cl::opt<bool> inPlace(
    "i",
    cl::desc("Whether to process the file in place, only applies if file is given"),
    cl::value_desc("inplace"), cl::init(false));
int main(int argc, const char **argv)
{
    cl::ParseCommandLineOptions(
        argc, argv,
        "A tool to format C"
        "code.\n\n"
        "If no arguments are specified, it formats the code from standard input\n"
        "and writes the result to the standard output.\n"
        "If <file>s are given, it reformats the files. If -i is specified\n"
        "together with <file>s, the files are edited in-place. Otherwise, the\n"
        "result is written to the standard output.\n");
    // read in file
    llvm::outs() << file.getValue() << "\n";
    if (file.empty())
    {
        llvm::errs() << "You must provide a file for now\n";
        return 1;
    }
    ifstream reader(file.getValue());
    stringstream buf;
    buf << reader.rdbuf();
    reader.close();
    string contents = buf.str();

    // format
    SourceManagerForFile sm(file.getValue(), contents);
    MinifyFormatter formatter(sm.get());
    Replacements replacements = formatter.process();

    // save replacements
    LangOptions lo;
    Rewriter rewriter(sm.get(), lo);
    if (!applyAllReplacements(replacements, rewriter))
    {
        llvm::outs() << "something went wrong??\n";
    }
    RewriteBuffer &buffer = rewriter.getEditBuffer(sm.get().getMainFileID());
    if (inPlace.getValue())
    {
        error_code ec;
        raw_fd_ostream out(file.getValue(), ec);
        buffer.write(out);
        out.close();
    }
    else
    {
        llvm::outs() << "writing to out.c" << "\n";
        error_code ec;
        raw_fd_ostream out("out.c", ec);
        buffer.write(out);
        out.close();
    }
}