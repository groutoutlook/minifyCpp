#include <format/minifyFormat.hpp>
using namespace clang;
using namespace clang::tooling;
MinifyFormatter::MinifyFormatter(SourceManager &sm) : sm(sm) {}

enum LastTokenType
{
    punctuator = 0, // if last token was a punctuator
    preprocessor,   // if last token was part of a preprocessor action
    BOF,            // if it's the beginning of the file
    other,          // other cases
};

bool isPunctuator(const Token &t)
{
    return t.isOneOf(
        tok::l_square,
        tok::r_square,
        tok::l_paren,
        tok::r_paren,
        tok::l_brace,
        tok::r_brace,
        tok::period,
        tok::ellipsis,
        tok::amp,
        tok::ampamp,
        tok::ampequal,
        tok::star,
        tok::starequal,
        tok::plus,
        tok::plusplus,
        tok::plusequal,
        tok::minus,
        tok::arrow,
        tok::minusminus,
        tok::minusequal,
        tok::tilde,
        tok::exclaim,
        tok::exclaimequal,
        tok::slash,
        tok::slashequal,
        tok::percent,
        tok::percentequal,
        tok::less,
        tok::lessless,
        tok::lessequal,
        tok::lesslessequal,
        tok::spaceship,
        tok::greater,
        tok::greatergreater,
        tok::greaterequal,
        tok::greatergreaterequal,
        tok::caret,
        tok::caretequal,
        tok::pipe,
        tok::pipepipe,
        tok::pipeequal,
        tok::question,
        tok::colon,
        tok::semi,
        tok::equal,
        tok::equalequal,
        tok::comma,
        tok::hash);
}
LastTokenType getTokenType(const Token &t)
{
    if (t.is(tok::hash))
    {
        return preprocessor;
    }
    else if (isPunctuator(t))
    {
        return punctuator;
    }
    return other;
}

Replacements MinifyFormatter::process()
{
    Replacements result;
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
    while (!tok.is(tok::eof))
    {
        // get info on cur token
        LastTokenType curTokenType = getTokenType(tok);

        // replace spaces between this token and the previous token
        SourceLocation replacementStart = prevLocation;
        SourceLocation replacementEnd = tok.getLocation();
        const CharSourceRange &range = CharSourceRange::getCharRange(SourceRange(replacementStart, replacementEnd));
        if (lastTokenType == BOF)
        {
        }
        else if (lastTokenType == preprocessor || curTokenType == preprocessor)
        {
            // need a newline between prev location and this location
            llvm::cantFail(result.add(Replacement(sm, range, "\n")));
        }
        else if (lastTokenType == punctuator || curTokenType == punctuator)
        {
            // no spaces between punctuators and things
            llvm::cantFail(result.add(Replacement(sm, range, "")));
        }
        else if (lastTokenType == other)
        {
            // both this and the previous are some sort of raw-identifiers
            // so use a space
            llvm::cantFail(result.add(Replacement(sm, range, " ")));
        }

        // if it was a preprocessor, skip till the end of the preprocessor
        if (tok.is(tok::hash))
        {
            lexer.LexFromRawLexer(tok);
            while (!tok.isAtStartOfLine() && !tok.is(tok::eof))
            {
                // update last location
                prevLocation = tok.getEndLoc();
                lexer.LexFromRawLexer(tok);
            }
        }
        else // if didn't skip, it's ok to use the value of tok here
        {
            prevLocation = tok.getEndLoc();
            lexer.LexFromRawLexer(tok);
        }
        lastTokenType = curTokenType;
    }
    // replace any whitespace between eof token and last token with empty
    SourceLocation replacementStart = prevLocation;
    SourceLocation replacementEnd = tok.getLocation();
    const CharSourceRange &range = CharSourceRange::getCharRange(SourceRange(replacementStart, replacementEnd));
    llvm::cantFail(result.add(Replacement(sm, range, "")));

    return result;
}