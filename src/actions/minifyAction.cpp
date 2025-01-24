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
pair<int, string> toSymbol(int i, set<string> &reserved)
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
    } while (keywords.find(result) != keywords.end() || reserved.find(result) != reserved.end());
    return {curNum, result};
}

// for now, we will not rename structs or enums or typedefs
// due to the fact that these are hard to trace in function pointer
// definitions

class StateManager
{
public:
    enum SymbolType
    {
        VAR_OR_FUNC = 0,
        ENUM,
        STRUCT,
    };

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

            ScopePair() : maxUsedSymbol(0) {};
            ScopePair(const ScopePair &other) : maxUsedSymbol(other.maxUsedSymbol) {};
        };

        map<SymbolType, ScopePair> usedSymbols;

        Scope(SourceLocation end) : end(end)
        {
            usedSymbols[VAR_OR_FUNC] = ScopePair{};
            usedSymbols[ENUM] = ScopePair{};
        }
        Scope(SourceLocation end, const Scope &other) : end(end)
        {
            usedSymbols[VAR_OR_FUNC] = ScopePair(other.usedSymbols.find(VAR_OR_FUNC)->second);
        }

        ScopePair &operator[](SymbolType tp)
        {
            return usedSymbols[tp];
        }
    };

    vector<Scope> scopes;            // pair of scope and when that scope ends
    map<QualType, int> recordNames;  // for struct/union name rewrites
    map<QualType, int> typedefNames; // for typedef name rewrites
    map<void *, int> enums;          // for enum name rewrites
    map<Decl *, int> declarations;   // for variables and functions
    ASTContext *context;

    void adjustScopes(SourceLocation cur)
    {
        // case where cur is in a different translation unit
        // since locations might be in different files, we compare them
        // with something more complicated than cur > scopes.back().end
        while (!context->getSourceManager().isBeforeInTranslationUnit(cur, scopes.back().end))
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
    StateManager(ASTContext *context) : context(context)
    {
        // start with a global scope
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
    string addSymbol(Decl *decl, SymbolType tp)
    {
        // first, adjust scopes
        adjustScopes(decl->getLocation());

        // store it in declarations
        Scope::ScopePair &relevant = scopes.back()[tp];
        int symbolNum = relevant.maxUsedSymbol;
        auto [nextSymbolNum, str] = toSymbol(symbolNum, scopes.front()[tp].externalSymbols); // external symbols only really apply from global scope
        declarations[decl->getCanonicalDecl()] = symbolNum;
        relevant.maxUsedSymbol = nextSymbolNum;
        return str;
    }
    string addSymbol(SourceLocation location, QualType tp, SymbolType stp)
    {
        // first, adjust scopes
        adjustScopes(location);

        // store it in declarations
        Scope::ScopePair &relevant = scopes.back()[stp];
        int symbolNum = relevant.maxUsedSymbol;
        auto [nextSymbolNum, str] = toSymbol(symbolNum, scopes.front()[stp].externalSymbols); // external symbols only really apply from global scope
        if (stp == ENUM)
        {
            enums[tp.getAsOpaquePtr()] = symbolNum;
        }
        else
        {
            errs() << "UNSUPPORTED AS OF NOW\n"; // TODO
        }
        relevant.maxUsedSymbol = nextSymbolNum;
        return str;
    }
    /**
     * @brief Adds a symbol that cannot be rewritten to the current scope
     *
     * @param decl the declaration that cannot be rewritten
     * @param symbol
     */
    void addExternalSymbol(Decl *decl, string symbol, SymbolType tp)
    {
        // first, adjust scopes
        adjustScopes(decl->getLocation());

        // then, add the symbol
        scopes.back()[tp].externalSymbols.emplace(symbol);
    }
    void addExternalSymbol(SourceLocation location, string symbol, SymbolType tp)
    {
        // first, adjust scopes
        adjustScopes(location);

        // then, add the symbol
        scopes.back()[tp].externalSymbols.emplace(symbol);
    }
    /**
     * @brief Get the abbreviated symbol for the given declaration, or fall
     * back to original
     *
     * @param decl
     * @param original
     * @return string
     */
    optional<string> getSymbol(Decl *decl, SymbolType tp)
    {
        // TODO - use different maps for var_or_func vs enum
        if (declarations.find(decl->getCanonicalDecl()) == declarations.end())
        {
            return optional<string>();
        }
        // external symbols only really apply from global scope
        return toSymbol(declarations[decl->getCanonicalDecl()], scopes.front()[tp].externalSymbols).second;
    }
    optional<string> getSymbol(QualType tp, SymbolType stp)
    {
        if (stp == ENUM && enums.find(tp.getAsOpaquePtr()) == enums.end())
        {
            return optional<string>();
        }
        return toSymbol(enums[tp.getAsOpaquePtr()], scopes.front()[stp].externalSymbols).second;
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
    // enums
    bool VisitEnumDecl(EnumDecl *decl)
    {
        auto p = getLoc(decl);
        if (p.second)
        {
            // register it and replace it
            QualType tp(decl->getTypeForDecl(), 0);
            string replacement = manager.addSymbol(decl->getLocation(), tp, StateManager::ENUM);
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), replacement)));
        }
        else
        {
            // register it
            manager.addExternalSymbol(decl->getLocation(), decl->getNameAsString(), StateManager::ENUM);
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
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl, StateManager::VAR_OR_FUNC))));
        }
        else
        {
            // external, maybe it affects us, maybe not
            manager.addExternalSymbol(decl, decl->getNameAsString(), StateManager::VAR_OR_FUNC);
        }
        return true;
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
        else
        {
            // TODO - log used struct names
        }
        return true;
    }
    // struct members
    bool VisitFieldDecl(FieldDecl *decl)
    {
        pair<SourceLocation, bool> p = getLoc(decl);
        if (p.second)
        {
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl, StateManager::VAR_OR_FUNC))));
        } // can't rewrite code outside of file
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
                cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl, StateManager::VAR_OR_FUNC))));
            }
            // then push a new scope based on current scope
            manager.pushCurScope(decl->getEndLoc());
        }
        else
        {
            // external, maybe it affects us, maybe not
            manager.addExternalSymbol(decl, decl->getNameAsString(), StateManager::VAR_OR_FUNC);
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
            cantFail(replacements->add(Replacement(context->getSourceManager(), decl->getLocation(), decl->getNameAsString().size(), manager.addSymbol(decl, StateManager::VAR_OR_FUNC))));
        }
        else
        {
            // external, but might be an accessible global variable
            // so add it as an external symbol
            manager.addExternalSymbol(decl, decl->getNameAsString(), StateManager::VAR_OR_FUNC);
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
            if (optional<string> replacement = manager.getSymbol(expr->getDecl(), StateManager::VAR_OR_FUNC))
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
            if (optional<string> replacement = manager.getSymbol(expr->getMemberDecl(), StateManager::VAR_OR_FUNC))
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
                    if (optional<string> replacement = manager.getSymbol(d.getFieldDecl(), StateManager::VAR_OR_FUNC))
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
            if (optional<string> replacement = manager.getSymbol(loc.getType(), StateManager::ENUM))
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
