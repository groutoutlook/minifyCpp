#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// constants
const set<string> keywords = {
    // "auto",
    "break",
    "case",
    "char",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "float",
    "for",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "register",
    "restrict",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while",
    "_Bool",
    "_Complex",
    "_Imaginary",
};

string toSymbol(int i)
{
    vector<char> result;
    while (i >= 0)
    {
        int tmp = i % 52; // use every lowercase and uppercase letter
        if (tmp < 26)
        {
            result.push_back('a' + tmp);
        }
        else
        {
            result.push_back('A' + tmp - 26);
        }
        i /= 52;
        i -= 1;
    }
    reverse(result.begin(), result.end());
    return string(result.begin(), result.end());
}

// for now, we will not rename structs or enums or typedefs
// due to the fact that these are hard to trace in function pointer
// definitions

// class that will deal with the state of the explorer,
// ie the stack of scopes
// function adds symbol to scope, then pushes a new scope set to current scope's
//      maxUsedSymbol
// struct/union adds symbol to scope, then pushes a new scope set to 0
// enum/typedefs/vars just add to the scope
// canonical declaration
struct Scope
{
    int maxUsedSymbol = 0; // exclusive
    Scope() {}
};
class StateManager
{
    vector<pair<Scope, SourceLocation>> scopes; // pair of scope and when that scope ends
    map<QualType, int> recordNames;             // for struct/union name rewrites
    map<QualType, int> typedefNames;            // for typedef name rewrites
    map<QualType, int> enumNames;               // for enum name rewrites
    map<Decl *, int> declarations;              // for variables and functions
    ASTContext *context;

public:
    StateManager(ASTContext *context) : context(context)
    {
        // start with a global scope
        // TODO - handle multiple files
        FileID mainFileId = context->getSourceManager().getMainFileID();
        scopes.push_back({Scope(), context->getSourceManager().getLocForEndOfFile(mainFileId)});
    };
    string addSymbol(Decl *decl)
    {
        int symbolNum = scopes.back().first.maxUsedSymbol++;
        declarations[decl->getCanonicalDecl()] = symbolNum;
        return toSymbol(symbolNum);
    }
    string getSymbol(Decl *decl, string original)
    {
        if (declarations.find(decl->getCanonicalDecl()) == declarations.end())
        {
            return original;
        }
        return toSymbol(declarations[decl->getCanonicalDecl()]);
    }
};

// notes:
// structs have MemberDecl's inside them
// all references to myStructInstance.member are MemberExpr's that point to
// the MemberDecl for that member
// thus, canonical decl for a variable is the address of the declaration
// canonicaltype is the type name (because type names are unique)
class MinifierVisitor
    : public RecursiveASTVisitor<MinifierVisitor>
{
private:
    Rewriter *rewriter;
    ASTContext *context;
    string sourceFileName;
    StateManager manager;

public:
    explicit MinifierVisitor(Rewriter *r, ASTContext *context, string sourceFileName)
        : rewriter(r), context(context), sourceFileName(sourceFileName), manager(context) {}

    template <typename T>
    pair<SourceLocation, bool> getLoc(T *decl)
    {
        SourceManager &m = context->getSourceManager();
        SourceLocation begin = decl->getBeginLoc();
        SourceLocation spellingLoc = m.getSpellingLoc(begin);
        // bool wasMacroArg = m.isMacroArgExpansion(begin);
        // bool wasMacro = wasMacroArg || m.isMacroBodyExpansion(begin);
        if (spellingLoc.isValid() &&
            m.getFilename(spellingLoc) == sourceFileName)
        {
            return {spellingLoc, true};
        }
        return {spellingLoc, false};
    }

    // structs
    bool VisitRecordDecl(RecordDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            // TODO - rewrite record names
        }
        return true;
    }
    // struct members
    bool VisitFieldDecl(FieldDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            rewriter->ReplaceText(decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl));
        }
        return true;
    }

    // type declaration
    bool VisitTypedefDecl(TypedefDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            // TODO - rewrite typedef names
        }
        return true;
    }

    // functions
    bool VisitFunctionDecl(FunctionDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            if (decl->getNameAsString() != "main")
            {
                // then rewrite this function too
                rewriter->ReplaceText(decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl));
            }
        }
        return true;
    }

    // regular variable declarations
    bool VisitVarDecl(VarDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            rewriter->ReplaceText(decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl));
        }
        return true;
    }

    // reference to variable
    bool VisitDeclRefExpr(DeclRefExpr *expr)
    {
        pair<SourceLocation, bool> p = getLoc(expr);
        if (p.second)
        {
            string originalName = expr->getDecl()->getNameAsString();
            rewriter->ReplaceText(p.first, originalName.size(), manager.getSymbol(expr->getDecl(), originalName));
        }
        return true;
    }
    // reference to member variable
    bool VisitMemberExpr(MemberExpr *expr)
    {
        pair<SourceLocation, bool> p = getLoc(expr);
        if (p.second)
        {
            string originalName = expr->getMemberDecl()->getNameAsString();
            rewriter->ReplaceText(expr->getExprLoc(), originalName.size(), manager.getSymbol(expr->getMemberDecl(), originalName));
        }
        return true;
    }

    // reference to member variable inside an initializer
    bool VisitDesignatedInitExpr(DesignatedInitExpr *expr)
    {
        auto p = getLoc(expr);
        if (p.second)
        {
            for (DesignatedInitExpr::Designator &d : expr->designators())
            {
                if (d.isFieldDesignator())
                {
                    StringRef originalName = d.getFieldName()->getName();
                    rewriter->ReplaceText(d.getFieldLoc(), originalName.size(), manager.getSymbol(d.getFieldDecl(), originalName.str()));
                }
            }
        }
        return true;
    }
};

class MinifierConsumer : public clang::ASTConsumer
{
private:
    MinifierVisitor visitor;

public:
    explicit MinifierConsumer(Rewriter *r, ASTContext *context, string sourceFileName)
        : visitor(r, context, sourceFileName) {}

    virtual void HandleTranslationUnit(clang::ASTContext &context) override
    {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
    }
};

class ExplorerAction : public clang::ASTFrontendAction
{
private:
    unique_ptr<Rewriter> rewriter;

public:
    virtual std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance &compiler,
                      llvm::StringRef inFile) override
    {
        llvm::outs() << "Processing " << inFile << "\n";
        rewriter = make_unique<Rewriter>(compiler.getASTContext().getSourceManager(), compiler.getASTContext().getLangOpts());
        return std::make_unique<MinifierConsumer>(
            rewriter.get(), &compiler.getASTContext(),
            inFile.str());
    }
    virtual void EndSourceFileAction() override
    {
        error_code ec;
        raw_fd_ostream out("out.c", ec);
        rewriter->getEditBuffer(rewriter->getSourceMgr().getMainFileID()).write(out);
        out.close();
    }
};

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory MyToolCategory("minifier options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nThe minifier tool is meant to be run on a single file.\n");

int main(int argc, const char **argv)
{
    auto expectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
    if (!expectedParser)
    {
        llvm::errs() << expectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &optionsParser = expectedParser.get();
    ClangTool tool(optionsParser.getCompilations(),
                   optionsParser.getSourcePathList());

    // double check that conditions are met before running our tool
    // TODO - handle multiple files
    if (optionsParser.getSourcePathList().size() == 0)
    {
        llvm::outs() << "No input files\n";
        return 2;
    }
    if (optionsParser.getSourcePathList().size() > 1)
    {
        llvm::outs() << "Too many input files, expected a single file\n";
        return 3;
    }
    int result = tool.run(newFrontendActionFactory<SyntaxOnlyAction>().get());
    if (result != 0)
    {
        llvm::outs() << "Failed to minify due to syntax errors\n";
        return 4;
    }

    // all conditions met, run our minifier
    return tool.run(newFrontendActionFactory<ExplorerAction>().get());
}