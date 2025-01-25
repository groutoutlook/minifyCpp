#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// notes:
// structs have MemberDecl's inside them
// all references to myStructInstance.member are MemberExpr's that point to
// the MemberDecl for that member
// thus, canonical decl for a variable is the address of the declaration
// canonicaltype is the type name (because type names are unique)
class ExplorerVisitor
    : public RecursiveASTVisitor<ExplorerVisitor>
{
private:
    ASTContext *context;
    string sourceFileName;

public:
    explicit ExplorerVisitor(ASTContext *context, string sourceFileName)
        : context(context), sourceFileName(sourceFileName) {}

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

    // enums
    bool VisitEnumDecl(EnumDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            outs() << "Found enum declaration of " << decl->getNameAsString()
                   << "(" << decl->getCanonicalDecl() << ") with qualtype " << QualType(decl->getTypeForDecl(), 0).getAsOpaquePtr() << " ";
            decl->getLocation().dump(context->getSourceManager());
        }
        return true;
    }
    bool VisitEnumConstantDecl(EnumConstantDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            outs() << "Found enum constant declaration of " << decl->getNameAsString()
                   << "(" << decl->getCanonicalDecl() << ")\n";
        }
        return true;
    }

    // structs
    bool VisitRecordDecl(RecordDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            outs() << "Found record declaration of " << decl->getNameAsString()
                   << "(" << decl->getCanonicalDecl() << ")\n";
            decl->getBeginLoc();
            decl->getEndLoc();
        }
        return true;
    }
    // struct members
    bool VisitFieldDecl(FieldDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            outs() << "Found field declaration of " << decl->getQualifiedNameAsString()
                   << "(" << decl->getCanonicalDecl() << ")\n";
            decl->getLocation().dump(context->getSourceManager());
            decl->dumpColor();
            decl->getType().dump();
        }
        return true;
    }

    // type declaration
    bool VisitTypedefDecl(TypedefDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            outs() << "Found typedef declaration of " << decl->getNameAsString()
                   << " (" << decl->getCanonicalDecl() << ")"
                   << " at ";
            decl->getLocation().dump(context->getSourceManager());
        }
        return true;
    }

    // functions
    bool VisitFunctionDecl(FunctionDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            outs() << "Found function declaration of "
                   << decl->getNameAsString() << " from ";
            decl->getBeginLoc().dump(context->getSourceManager());
            outs() << "to ";
            decl->getEndLoc().dump(context->getSourceManager());
        }
        return true;
    }
    bool VisitCompoundStmt(CompoundStmt *stmt)
    {
        auto p = getLoc(stmt);
        if (p.second)
        {
            outs() << "found compound statement from ";
            stmt->getBeginLoc().dump(context->getSourceManager());
            outs() << "to ";
            stmt->getEndLoc().dump(context->getSourceManager());
        }
        return true;
    }

    // regular variable declarations
    bool VisitVarDecl(VarDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {

            outs() << "Found variable declaration of "
                   << decl->getNameAsString() << " as type " << decl->getType()
                   << "(found at " << decl->getType().getCanonicalType() << ")\n";
        }
        return true;
    }

    // reference to variable
    bool VisitDeclRefExpr(DeclRefExpr *expr)
    {
        auto p = getLoc(expr);
        // outs() << "other stuff: ";
        // context->getSourceManager().getExpansionLoc(expr->getLocation()).dump(context->getSourceManager());
        // context->getSourceManager().getSpellingLoc(expr->getLocation()).dump(context->getSourceManager());
        // outs() << "validity: " << expr->getLocation().isValid() << ", file id: " << (context->getSourceManager().getFileID(expr->getLocation()) == context->getSourceManager().getMainFileID())
        //              << (context->getSourceManager().getFileID(context->getSourceManager().getFileLoc(expr->getLocation())) == context->getSourceManager().getMainFileID()) << "\n";
        // outs() << "is macro arg: " << context->getSourceManager().isMacroArgExpansion(expr->getLocation()) << " is body expansion: " << context->getSourceManager().isMacroBodyExpansion(expr->getLocation()) << "\n";
        if (p.second)
        {
            outs() << "Found expression reference to "
                   << expr->getDecl()->getCanonicalDecl() << "\n";
            outs() << "This expression references " << expr->getDecl()->getNameAsString() << "\n";
            expr->dumpColor();
            expr->getLocation().dump(context->getSourceManager());
        }
        else
        {
            // outs() << "This expression wasn't valid?" << "\n";
            // expr->dumpColor();
            // expr->getLocation().dump(context->getSourceManager());
        }
        return true;
    }
    // reference to member variable
    bool VisitMemberExpr(MemberExpr *expr)
    {
        auto p = getLoc(expr);
        if (p.second)
        {
            outs() << "Found member reference to "
                   << expr->getMemberDecl()->getCanonicalDecl()
                   << " which is " << expr->getMemberDecl()->getQualifiedNameAsString() << "\n";
        }
        return true;
    }
    // reference to member variable inside an initializer
    bool VisitDesignatedInitExpr(DesignatedInitExpr *expr)
    {
        auto p = getLoc(expr);
        if (p.second)
        {
            outs() << "Found designated initializer at ";
            p.first.dump(context->getSourceManager());
            for (DesignatedInitExpr::Designator &d : expr->designators())
            {
                outs() << "designator is ";
                if (d.isFieldDesignator())
                {
                    outs() << "field " << d.getFieldName()->getName() << " with location ";
                    d.getFieldLoc().dump(context->getSourceManager());
                }
                else if (d.isArrayDesignator())
                {
                    outs() << "array designator\n";
                }
                else if (d.isArrayRangeDesignator())
                {
                    outs() << "array range designator\n";
                }
                else
                {
                    outs() << "unknown?!?!?\n";
                }
            }
        }
        return true;
    }

    // types
    bool VisitEnumTypeLoc(EnumTypeLoc loc)
    {
        outs() << "got enum type loc for " << loc.getDecl()->getNameAsString() << " (" << loc.getType().getAsOpaquePtr() << ") at ";
        loc.getBeginLoc().dump(context->getSourceManager());
        return true;
    }
    bool VisitRecordTypeLoc(RecordTypeLoc loc)
    {

        outs() << "got record type loc for " << loc.getDecl()->getNameAsString() << " (" << loc.getType().getAsOpaquePtr() << ") at ";
        loc.getBeginLoc().dump(context->getSourceManager());
        return true;
    }
    bool VisitTypedefTypeLoc(TypedefTypeLoc loc)
    {
        outs() << "got typedef type loc for " << loc.getTypedefNameDecl()->getNameAsString() << " at ";
        loc.getBeginLoc().dump(context->getSourceManager());
        return true;
    }
};

class FindNamedClassConsumer : public clang::ASTConsumer
{
private:
    ExplorerVisitor visitor;

public:
    explicit FindNamedClassConsumer(ASTContext *context, string sourceFileName)
        : visitor(context, sourceFileName) {}

    virtual void HandleTranslationUnit(clang::ASTContext &context) override
    {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
    }
};

class ExplorerPPCallbacks : public PPCallbacks
{
private:
    shared_ptr<vector<StringRef>> definitions;

public:
    ExplorerPPCallbacks(const shared_ptr<vector<StringRef>> &definitions) : definitions(definitions) {};
    virtual void MacroDefined(const Token &macroNameTok, const MacroDirective *MD) override
    {
        StringRef name = macroNameTok.getIdentifierInfo()->getName();
        definitions->push_back(name);
    }
};

class ExplorerAction : public clang::ASTFrontendAction
{
public:
    virtual std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance &compiler,
                      llvm::StringRef inFile) override
    {
        outs() << "Processing " << inFile << "\n";
        shared_ptr<vector<StringRef>> definitions = make_shared<vector<StringRef>>();
        compiler.getPreprocessor().addPPCallbacks(make_unique<ExplorerPPCallbacks>(definitions));
        return std::make_unique<FindNamedClassConsumer>(&compiler.getASTContext(),
                                                        inFile.str());
    }
};

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

int main(int argc, const char **argv)
{
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
    if (!ExpectedParser)
    {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());
    for (const auto &source : OptionsParser.getSourcePathList())
    {
        cout << "Examining " << source << endl;
    }

    return Tool.run(newFrontendActionFactory<ExplorerAction>().get());
}