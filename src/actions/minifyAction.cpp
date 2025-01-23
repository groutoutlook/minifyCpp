#include <actions/minifyAction.hpp>
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

/**
 * @brief Calculates the next number that can be used for an identifier
 *
 * @param i the current number requested
 * @return pair<int, string> a pair containing (nextNumber, identifier)
 */
pair<int, string> toSymbol(int i)
{
    int curNum = i;
    string result;
    do
    {
        i = curNum;
        vector<char> resultArr;
        while (i >= 0)
        {
            int tmp = i % 52; // use every lowercase and uppercase letter
            if (tmp < 26)
            {
                resultArr.push_back('a' + tmp);
            }
            else
            {
                resultArr.push_back('A' + tmp - 26);
            }
            i /= 52;
            i -= 1;
        }
        reverse(resultArr.begin(), resultArr.end());
        result = string(resultArr.begin(), resultArr.end());
        ++curNum;
    } while (keywords.find(result) != keywords.end());
    return {curNum, result};
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
    SourceLocation end;
    int maxUsedSymbol; // exclusive
    Scope(SourceLocation end, int maxUsedSymbol = 0) : end(end), maxUsedSymbol(maxUsedSymbol) {}
};
class StateManager
{
    vector<Scope> scopes;            // pair of scope and when that scope ends
    map<QualType, int> recordNames;  // for struct/union name rewrites
    map<QualType, int> typedefNames; // for typedef name rewrites
    map<QualType, int> enumNames;    // for enum name rewrites
    map<Decl *, int> declarations;   // for variables and functions
    ASTContext *context;

    void adjustScopes(SourceLocation cur)
    {
        while (cur > scopes.back().end)
        {
            scopes.pop_back();
        }
    }

public:
    StateManager(ASTContext *context) : context(context)
    {
        // start with a global scope
        // TODO - handle multiple files
        FileID mainFileId = context->getSourceManager().getMainFileID();
        scopes.push_back(Scope(context->getSourceManager().getLocForEndOfFile(mainFileId)));
    };
    /**
     * @brief Adds a symbol for the declaration to the current scope
     *
     * @param decl the declaration. It should be a new declaration not already added with `addSymbol`.
     *       If it already exists in the scope, then its existing symbol is replaced
     * @return string
     */
    string addSymbol(Decl *decl)
    {
        // first, adjust scopes
        adjustScopes(decl->getLocation());

        // store it in declarations
        int symbolNum = scopes.back().maxUsedSymbol;
        auto [nextSymbolNum, str] = toSymbol(symbolNum);
        declarations[decl->getCanonicalDecl()] = symbolNum;
        scopes.back().maxUsedSymbol = nextSymbolNum;
        return str;
    }
    /**
     * @brief Get the abbreviated symbol for the given declaration, or fall
     * back to original
     *
     * @param decl
     * @param original
     * @return string
     */
    string getSymbol(Decl *decl, string original)
    {
        if (declarations.find(decl->getCanonicalDecl()) == declarations.end())
        {
            return original;
        }
        return toSymbol(declarations[decl->getCanonicalDecl()]).second;
    }
    /**
     * @brief Push an empty scope onto the scope stack
     *
     * @param end the source location, inclusive, of when the scope should end
     *
     * @details This method adds a scope that will be completely empty, meaning that symbols will start again
     * from 0, no matter what the current scope's maxSymbol is presently
     *
     */
    void pushEmptyScope(SourceLocation end)
    {
        adjustScopes(end);
        scopes.push_back(Scope(end));
    }
    /**
     * @brief Push a scope identical to the current scope onto the scope stack
     *
     * @end the source location, inclusive, of when the scope should end
     *
     * @details This method adds a scope that has the same maxSymbol as the current scope
     *
     */
    void pushCurScope(SourceLocation end)
    {
        adjustScopes(end);
        scopes.push_back(Scope(end, scopes.back().maxUsedSymbol));
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
    Replacements *replacements;
    ASTContext *context;
    string sourceFileName;
    StateManager manager;

public:
    explicit MinifierVisitor(Replacements *r, ASTContext *context, string sourceFileName)
        : replacements(r), context(context), sourceFileName(sourceFileName), manager(context) {}

    template <typename T>
    pair<SourceLocation, bool> getLoc(T *decl)
    {
        SourceManager &m = context->getSourceManager();
        SourceLocation begin = decl->getBeginLoc();
        SourceLocation spellingLoc = m.getSpellingLoc(begin);
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
            // push a new scope since the struct is its own scope
            manager.pushEmptyScope(decl->getEndLoc());
        }
        return true;
    }
    // struct members
    bool VisitFieldDecl(FieldDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl))));
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

    // compound statements (simply add a cur scope)
    bool VisitCompoundStmt(CompoundStmt *s)
    {
        pair<SourceLocation, bool> p = getLoc(s);
        if (p.second)
        {
            // push
            manager.pushCurScope(s->getEndLoc());
        }
        return true;
    }

    // functions
    bool VisitFunctionDecl(FunctionDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            // do replacement first (since function needs to be visible to following items)
            if (decl->getNameAsString() != "main")
            {
                // then rewrite this function too
                cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl))));
            }
            // then push a new scope based on current scope
            manager.pushCurScope(decl->getEndLoc());
        }
        return true;
    }

    // regular variable declarations
    bool VisitVarDecl(VarDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl))));
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
            cantFail(replacements->add(Replacement(context->getSourceManager(), p.first, originalName.size(), manager.getSymbol(expr->getDecl(), originalName))));
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
            cantFail(replacements->add(Replacement(context->getSourceManager(), expr->getExprLoc(), originalName.size(), manager.getSymbol(expr->getMemberDecl(), originalName))));
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
                    cantFail(replacements->add(Replacement(context->getSourceManager(), d.getFieldLoc(), originalName.size(), manager.getSymbol(d.getFieldDecl(), originalName.str()))));
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
    explicit MinifierConsumer(Replacements *r, ASTContext *context, string sourceFileName)
        : visitor(r, context, sourceFileName) {}

    virtual void HandleTranslationUnit(clang::ASTContext &context) override
    {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
    }
};
MinifierAction::MinifierAction(Replacements *replacements) : replacements(replacements) {};
std::unique_ptr<clang::ASTConsumer>
MinifierAction::CreateASTConsumer(clang::CompilerInstance &compiler,
                                  llvm::StringRef inFile)
{
    return std::make_unique<MinifierConsumer>(
        replacements, &compiler.getASTContext(),
        inFile.str());
}

std::unique_ptr<clang::tooling::FrontendActionFactory> MinifierAction::newMinifierAction(clang::tooling::Replacements *replacements)
{
    class MinifierActionFactory : public FrontendActionFactory
    {
    public:
        Replacements *replacements;
        MinifierActionFactory(Replacements *rs) : replacements(rs) {};
        std::unique_ptr<FrontendAction> create() override
        {
            return std::make_unique<MinifierAction>(replacements);
        }
    };
    return std::make_unique<MinifierActionFactory>(replacements);
}
