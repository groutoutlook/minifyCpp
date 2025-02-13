#include <util/symbols.hpp>
#include <vector>
#include <algorithm>

using namespace std;

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