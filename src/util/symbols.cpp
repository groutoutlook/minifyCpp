#include <util/symbols.hpp>
#include <vector>
#include <algorithm>

using namespace std;
using namespace clang;

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