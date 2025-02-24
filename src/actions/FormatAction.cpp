#include <actions/FormatAction.hpp>
#include <util/symbols.hpp>
#include <clang/Frontend/CompilerInstance.h>
#include <deque>
using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace std;
FormatAction::FormatAction(clang::tooling::Replacements *replacements) : replacements(replacements) {}

enum LastTokenType
{
    punctuator = 0, // if last token was a punctuator
    BOF,            // if it's the beginning of the file
    other,          // other cases
};

LastTokenType getTokenType(const Token &t)
{
    if (isPunctuator(t))
    {
        return punctuator;
    }
    return other;
}

void FormatAction::ExecuteAction()
{
    SourceManager &sm = getCompilerInstance().getSourceManager();
    LangOptions opts;
    Lexer lexer(sm.getMainFileID(), sm.getMemoryBufferForFileOrFake(*sm.getFileEntryRefForID(sm.getMainFileID())), sm, opts);

    // in order to successfully minify a file, we need to remove spaces and comments
    // lexer skips comments
    // and then in order to remove spaces, we can simply apply a replacement
    // between the current token and the previous token
    // that replacement should be an empty string if the current token or the previous token
    // is a punctuator
    // otherwise, a single space
    Token tok;
    lexer.LexFromRawLexer(tok); // take first token into tok
    LastTokenType lastTokenType = BOF;
    SourceLocation prevLocation = sm.getLocForStartOfFile(sm.getMainFileID());
    deque<Token> prevTokens;

    bool wasPP = false; // true if last thing was from a preprocessor
    while (!tok.is(tok::eof))
    {
        // get info on cur token
        LastTokenType curTokenType = getTokenType(tok);
        bool isFirstPP = tok.is(tok::hash) && tok.isAtStartOfLine();

        // replace spaces between this token and the previous token
        SourceLocation replacementStart = prevLocation;
        SourceLocation replacementEnd = tok.getLocation();
        const CharSourceRange &range = CharSourceRange::getCharRange(SourceRange(replacementStart, replacementEnd));
        if (lastTokenType == BOF)
        {
            // no spaces between start of file and first token
            cantFail(replacements->add(Replacement(sm, range, "")));
        }
        else if (isFirstPP || (wasPP && tok.isAtStartOfLine()))
        {
            if (wasPP && tok.isAtStartOfLine())
            {
                wasPP = false;
            }
            // need a newline between prev location and this location
            cantFail(replacements->add(Replacement(sm, range, "\n")));
        }
        else if (lastTokenType == punctuator || curTokenType == punctuator)
        {
            // currently in a preprocessor, so need to be careful about moving
            // punctuators here
            // specifically, if the past 3 tokens are
            // '#', 'define', and (some identifier), then that means that
            // this is a define and we adjust space to either 1 space or none,
            // depending on whether there's a space already or not
            if (wasPP && prevTokens.size() == 3 &&
                prevTokens.front().isAtStartOfLine() && prevTokens.front().is(tok::hash) &&              // first was hash
                prevTokens[1].is(tok::raw_identifier) && prevTokens[1].getRawIdentifier() == "define" && // then define
                prevTokens[2].is(tok::raw_identifier) &&                                                 // then some identifier (followed by this token, a punctuator)
                replacementStart != replacementEnd)                                                      // there's some space between the defined thing and this punctuator
            {
                // then basically there's some amount of whitespace in between
                // we just replace that x amount of whitespaces with 1 single whitespace
                cantFail(replacements->add(Replacement(sm, range, " ")));
            }
            else
            {
                // normally, no spaces between punctuators and things
                cantFail(replacements->add(Replacement(sm, range, "")));
            }
        }
        else if (lastTokenType == other)
        {
            // both this and the previous are some sort of raw-identifiers
            // so use a space
            cantFail(replacements->add(Replacement(sm, range, " ")));
        }

        // adjust deque
        prevTokens.push_back(tok);
        while (prevTokens.size() > 3)
        {
            prevTokens.pop_front();
        }

        // advance to next token
        prevLocation = tok.getEndLoc();
        lexer.LexFromRawLexer(tok);
        lastTokenType = curTokenType;
        wasPP = wasPP || isFirstPP;
    }
    // replace any whitespace between eof token and last token with empty
    SourceLocation replacementStart = prevLocation;
    SourceLocation replacementEnd = tok.getLocation();
    const CharSourceRange &range = CharSourceRange::getCharRange(SourceRange(replacementStart, replacementEnd));
    cantFail(replacements->add(Replacement(sm, range, "")));
}

// adapter
unique_ptr<FrontendActionFactory> FormatAction::newFormatAction(Replacements *replacements)
{
    class Adapter : public FrontendActionFactory
    {
    private:
        Replacements *replacements;

    public:
        Adapter(Replacements *replacements) : replacements(replacements) {}
        std::unique_ptr<FrontendAction> create() override
        {
            return std::make_unique<FormatAction>(replacements);
        }
    };
    return std::make_unique<Adapter>(replacements);
}