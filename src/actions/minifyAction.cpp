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
    "auto",
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
pair<int, string> toSymbol(int i, const set<string> &reserved, set<string> *defines)
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

        // keep going while this is a keyword or reserved identifier
    } while (keywords.find(result) != keywords.end() || reserved.find(result) != reserved.end() || defines->find(result) != defines->end());
    return {curNum, result};
}

// for now, we will not rename structs or enums or typedefs
// due to the fact that these are hard to trace in function pointer
// definitions

class StateManager
{
private:
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
        struct ScopePair
        {
            int maxUsedSymbol;
            set<string> externalSymbols;

            ScopePair(int maxUsedSymbol = 0) : maxUsedSymbol(maxUsedSymbol) {};
        };

        ScopePair declarations;
        ScopePair typeNames;

        Scope(SourceLocation end) : end(end) {}
        Scope(SourceLocation end, const Scope &other) : end(end),
                                                        declarations(other.declarations.maxUsedSymbol),
                                                        typeNames(other.typeNames.maxUsedSymbol) {};
    };

    vector<Scope> scopes;          // pair of scope and when that scope ends
    map<void *, int> typeNames;    // for enum/(struct/union) name rewrites
    map<Decl *, int> declarations; // for variables and functions and typedefs
    set<string> *definitions;      // for reserved macro-defined identifiers
    ASTContext *context;

    void adjustScopes(SourceLocation cur)
    {
        // case where cur is in a different translation unit
        // since locations might be in different files, we compare them
        // with something more complicated than cur > scopes.back().end
        while (context->getSourceManager().isBeforeInTranslationUnit(scopes.back().end, cur))
        {
            scopes.pop_back();
        }
    }

public:
    /**
     * @brief StateManager constructor
     *
     * Creates a StateManager to manage the state of the MinifierAction's
     * explorer. The StateManager will keep track of the current scope.
     * The current scope is the scope most recently added to the scope stack.
     * The scope stack is a stack of scopes, each with a start and end location.
     * When a new scope is added to the scope stack, it is added on top of the
     * current scope, and the current scope is set to the new scope.
     * When a declaration is added to the current scope, it is added to the
     * current scope's set of symbols.
     * The StateManager also keeps track of all of the symbols added to all of the
     * scopes in the scope stack.
     * @param context the ASTContext to use with this StateManager
     */
    StateManager(set<string> *definitions, ASTContext *context) : definitions(definitions), context(context)
    {
        // start with a global scope
        FileID mainFileId = context->getSourceManager().getMainFileID();
        scopes.push_back(Scope(context->getSourceManager().getLocForEndOfFile(mainFileId)));
    };
    /**
     * @brief Adds a declaration (variable/function/typedef) to the current scope
     *
     * @param decl the declaration. It should be a new declaration not already added with `addSymbol`.
     *       If it already exists in the scope, then its existing symbol is replaced
     * @return string
     */
    string addDecl(Decl *decl)
    {
        // first, adjust scopes
        adjustScopes(decl->getLocation());

        // store it in declarations
        Scope::ScopePair &relevant = scopes.back().declarations;
        int symbolNum = relevant.maxUsedSymbol;
        auto [nextSymbolNum, str] = toSymbol(symbolNum, scopes.front().declarations.externalSymbols, definitions); // external symbols only really apply from global scope
        declarations[decl->getCanonicalDecl()] = symbolNum;
        relevant.maxUsedSymbol = nextSymbolNum;
        return str;
    }
    /**
     * @brief Adds a type (struct name or enum name) to the current scope
     *
     * @param location the type's location (used to adjust scopes)
     * @param tp the type to add
     * @return string
     */
    string addType(SourceLocation location, QualType tp)
    {
        // first, adjust scopes
        adjustScopes(location);

        // store it in declarations
        Scope::ScopePair &relevant = scopes.back().typeNames;
        int symbolNum = relevant.maxUsedSymbol;
        auto [nextSymbolNum, str] = toSymbol(symbolNum, scopes.front().typeNames.externalSymbols, definitions); // external symbols only really apply from global scope
        typeNames[tp.getAsOpaquePtr()] = symbolNum;
        relevant.maxUsedSymbol = nextSymbolNum;
        return str;
    }
    /**
     * @brief Adds a symbol that cannot be rewritten to the current scope's declarations
     *
     * @param decl the declaration that cannot be rewritten
     * @param symbol the declaration's name
     */
    void addExternalDecl(Decl *decl, string symbol)
    {
        // first, adjust scopes
        adjustScopes(decl->getLocation());

        // then, add the symbol
        scopes.back().declarations.externalSymbols.emplace(symbol);
    }
    /**
     * @brief Adds a symbol that cannot be rewritten to the current scope's types
     *
     * @param location the location of the type (for adjusting the scope)
     * @param symbol the type's name
     */
    void addExternalType(SourceLocation location, string symbol)
    {
        // first, adjust scopes
        adjustScopes(location);

        // then, add the symbol
        scopes.back().typeNames.externalSymbols.emplace(symbol);
    }
    /**
     * @brief Get the abbreviated symbol for the given declaration, or fall
     * back to original
     *
     * @param decl
     * @return string
     */
    optional<string> getDeclAbbr(Decl *decl)
    {
        if (declarations.find(decl->getCanonicalDecl()) == declarations.end())
        {
            return optional<string>();
        }
        // external symbols only really apply from global scope
        return toSymbol(declarations[decl->getCanonicalDecl()], scopes.front().declarations.externalSymbols, definitions).second;
    }
    optional<string> getTypeAbbr(QualType tp)
    {
        if (typeNames.find(tp.getAsOpaquePtr()) == typeNames.end())
        {
            return optional<string>();
        }
        return toSymbol(typeNames[tp.getAsOpaquePtr()], scopes.front().typeNames.externalSymbols, definitions).second;
    }

    // enums

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
        scopes.push_back(Scope(end, scopes.back()));
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
    explicit MinifierVisitor(set<string> *definitions, Replacements *r, ASTContext *context, string sourceFileName)
        : replacements(r), context(context), sourceFileName(sourceFileName), manager(definitions, context) {}

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
    // enums
    bool VisitEnumDecl(EnumDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            // register it and replace it
            QualType tp(decl->getTypeForDecl(), 0);
            string replacement = manager.addType(decl->getLocation(), tp);
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), replacement)));
        }
        else
        {
            // external enum type name, maybe it affects us, maybe not; either way, register it
            manager.addExternalType(decl->getLocation(), decl->getNameAsString());
        }
        return true;
    }
    bool VisitEnumConstantDecl(EnumConstantDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            // need to add this to known declarations
            // and then also replace this
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addDecl(decl))));
        }
        else
        {
            // external variable, maybe it affects us, maybe not
            manager.addExternalDecl(decl, decl->getNameAsString());
        }
        return true;
    }

    // structs
    bool VisitRecordDecl(RecordDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            // rewrite record name
            QualType tp(decl->getTypeForDecl(), 0);
            string replacement = manager.addType(decl->getLocation(), tp);
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), replacement)));

            // push a new scope since the struct is its own scope
            manager.pushEmptyScope(decl->getEndLoc());
        }
        else
        {
            // external type that may/may not affects us
            manager.addExternalType(decl->getLocation(), decl->getNameAsString());
        }
        return true;
    }
    // struct members
    bool VisitFieldDecl(FieldDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addDecl(decl))));
        } // can't rewrite code outside of file
        return true;
    }

    // type declaration
    bool VisitTypedefDecl(TypedefDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            // add symbol and rewrite it
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addDecl(decl))));
        }
        else
        {
            // add external typedef to known symbols
            manager.addExternalDecl(decl, decl->getNameAsString());
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
                cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addDecl(decl))));
            }
            // then push a new scope based on current scope
            manager.pushCurScope(decl->getEndLoc());
        }
        else
        {
            // external, maybe it affects us, maybe not
            manager.addExternalDecl(decl, decl->getNameAsString());
            // and also push a new scope
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
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addDecl(decl))));
        }
        else
        {
            // external, but might be an accessible global variable
            // so add it as an external symbol
            manager.addExternalDecl(decl, decl->getNameAsString());
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
            if (optional<string> replacement = manager.getDeclAbbr(expr->getDecl()))
            {
                cantFail(replacements->add(Replacement(context->getSourceManager(), p.first, originalName.size(), *replacement)));
            }
        } // can't rewrite code outside of file
        return true;
    }
    // reference to member variable
    bool VisitMemberExpr(MemberExpr *expr)
    {
        pair<SourceLocation, bool> p = getLoc(expr);
        if (p.second)
        {
            string originalName = expr->getMemberDecl()->getNameAsString();
            if (optional<string> replacement = manager.getDeclAbbr(expr->getMemberDecl()))
            {
                cantFail(replacements->add(Replacement(context->getSourceManager(), expr->getExprLoc(), originalName.size(), *replacement)));
            }
        } // can't rewrite code outside of file
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
                    if (optional<string> replacement = manager.getDeclAbbr(d.getFieldDecl()))
                    {
                        cantFail(replacements->add(Replacement(context->getSourceManager(), d.getFieldLoc(), originalName.size(), *replacement)));
                    }
                }
            }
        } // can't rewrite code outside of file
        return true;
    }

    // enum types
    bool VisitEnumTypeLoc(EnumTypeLoc loc)
    {
        if (context->getSourceManager().isInMainFile(loc.getBeginLoc()))
        {
            string name = loc.getDecl()->getNameAsString();
            // try replacement
            if (optional<string> replacement = manager.getTypeAbbr(loc.getType()))
            {
                cantFail(replacements->add(Replacement(context->getSourceManager(), loc.getBeginLoc(), name.size(), *replacement)));
            }
        }
        return true;
    }

    // record types
    bool VisitRecordTypeLoc(RecordTypeLoc loc)
    {
        if (context->getSourceManager().isInMainFile(loc.getBeginLoc()))
        {
            string name = loc.getDecl()->getNameAsString();
            // try replacement
            if (optional<string> replacement = manager.getTypeAbbr(loc.getType()))
            {
                cantFail(replacements->add(Replacement(context->getSourceManager(), loc.getBeginLoc(), name.size(), *replacement)));
            }
        }
        return true;
    }

    // typedef types
    bool VisitTypedefTypeLoc(TypedefTypeLoc loc)
    {
        if (context->getSourceManager().isInMainFile(loc.getBeginLoc()))
        {
            string name = loc.getTypedefNameDecl()->getNameAsString();
            // try replacement
            if (optional<string> replacement = manager.getDeclAbbr(loc.getTypedefNameDecl()))
            {
                cantFail(replacements->add(Replacement(context->getSourceManager(), loc.getBeginLoc(), name.size(), *replacement)));
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
    explicit MinifierConsumer(set<string> *definitions, Replacements *r, ASTContext *context, string sourceFileName)
        : visitor(definitions, r, context, sourceFileName) {}

    virtual void HandleTranslationUnit(clang::ASTContext &context) override
    {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
    }
};
MinifierAction::MinifierAction(Replacements *replacements, set<string> *definitions) : replacements(replacements), definitions(definitions) {};
std::unique_ptr<clang::ASTConsumer>
MinifierAction::CreateASTConsumer(clang::CompilerInstance &compiler,
                                  llvm::StringRef inFile)
{
    return std::make_unique<MinifierConsumer>(
        definitions, replacements, &compiler.getASTContext(),
        inFile.str());
}

std::unique_ptr<clang::tooling::FrontendActionFactory> MinifierAction::newMinifierAction(clang::tooling::Replacements *replacements, set<string> *definitions)
{
    class MinifierActionFactory : public FrontendActionFactory
    {
    public:
        Replacements *replacements;
        set<string> *definitions;
        MinifierActionFactory(Replacements *rs, set<string> *definitions) : replacements(rs), definitions(definitions) {};
        std::unique_ptr<FrontendAction> create() override
        {
            return std::make_unique<MinifierAction>(replacements, definitions);
        }
    };
    return std::make_unique<MinifierActionFactory>(replacements, definitions);
}
