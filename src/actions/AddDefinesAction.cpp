#include <util/symbols.hpp>
#include <actions/AddDefinesAction.hpp>
#include <clang/Frontend/CompilerInstance.h>
#include <algorithm>
#include <sstream>

// namespaces
using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace std;

const int DEFINE_WEIGHT = 10;

// ctor
AddDefinesAction::AddDefinesAction(int firstUnusedSymbol, bool niceMacros, Replacements *replacements) : firstUnusedSymbol(firstUnusedSymbol), niceMacros(niceMacros), replacements(replacements) {}

struct TokenInfo
{
    string spelling;
    bool isPP;
    bool isPunctuator;
    int weight; // gets initialized by program later

    // ctor
    TokenInfo() : spelling(""), isPP(false), isPunctuator(false) {}
    TokenInfo(string spelling, bool isPP, bool isPunctuator) : spelling(spelling), isPP(isPP), isPunctuator(isPunctuator) {}
    friend ostream &operator<<(ostream &o, const TokenInfo &t) { return o << t.spelling; }

    // for map
    bool operator<(const TokenInfo &other) const { return spelling < other.spelling; }
};

// returns tokens and the end location
pair<vector<TokenInfo>, SourceLocation> getTokens(SourceManager &sm)
{
    // initialize lexer and result
    vector<TokenInfo> tokens;
    LangOptions lo;
    Lexer lexer(sm.getMainFileID(), sm.getMemoryBufferForFileOrFake(*sm.getFileEntryRefForID(sm.getMainFileID())), sm, lo);

    // initialization
    Token tok;
    lexer.LexFromRawLexer(tok);
    while (!tok.is(tok::eof))
    {
        string spelling = lexer.getSpelling(tok, sm, lo);
        bool isPP = false;
        bool punctuator = isPunctuator(tok);
        if (tok.isAtStartOfLine() && tok.is(tok::hash))
        {
            // combine everything in this preprocessor
            // into one token
            spelling = "\n" + spelling; // prepend the newline
            isPP = true;
            SourceLocation prevLocation = tok.getEndLoc();
            lexer.LexFromRawLexer(tok);
            while (!tok.is(tok::eof) && !tok.isAtStartOfLine())
            {
                // figure out if need to add a space
                SourceLocation curLocation = tok.getLocation();
                if (curLocation != prevLocation)
                {
                    spelling += " ";
                }
                spelling += lexer.getSpelling(tok, sm, lo);

                // advance onto the next token
                prevLocation = curLocation;
                lexer.LexFromRawLexer(tok);
            }
            spelling += "\n"; // add the newline after the preprocessor directive
        }
        else
        {
            lexer.LexFromRawLexer(tok);
        }
        tokens.push_back(TokenInfo(spelling, isPP, punctuator && !isPP));
    }

    return {tokens, tok.getLocation()};
}
vector<int> sortCyclicShifts(const vector<int> &arr)
{
    int n = arr.size();

    // p holds permuation (order)
    // c holds equivalence class
    vector<int> p(n), c(n);

    // sort by first letter
    // then use that knowledge to combine 2 strings of length l
    // to make a string of length l*2

    // sort by first letter
    for (int i = 0; i < n; ++i)
    {
        p[i] = i;
    }
    std::sort(p.begin(), p.end(), [&arr](int a, int b)
              { return arr[a] < arr[b]; });

    // now fill in equivalency classes
    c[p[0]] = 0;
    int classes = 1;
    for (int i = 1; i < n; ++i)
    {
        if (arr[p[i]] != arr[p[i - 1]])
        {
            // new equivalency class
            ++classes;
        }
        c[p[i]] = classes - 1;
    }

    // now we can combine strings
    vector<int> pn(n);
    vector<int> cn(n);
    vector<int> counts(n); // preallocate space for the counts array used for the count sort
    for (int k = 0; (1 << k) < n; ++k)
    {
        // first, populate p_n
        for (int i = 0; i < n; ++i)
        {
            pn[i] = p[i] - (1 << k);
            if (pn[i] < 0)
            {
                pn[i] += n;
            }
        }

        // then sort by the item
        // but first, clear (only the items that we will use in) counts
        fill(counts.begin(), counts.begin() + classes, 0);
        for (int i = 0; i < n; ++i)
        {
            counts[c[pn[i]]]++;
        }
        // accumulate for counting sort
        for (int i = 1; i < classes; ++i)
        {
            counts[i] += counts[i - 1];
        }
        // finish count sort
        for (int i = n - 1; i > -1; --i)
        {
            p[--counts[c[pn[i]]]] = pn[i];
        }

        cn[p[0]] = 0;
        classes = 1;
        for (int i = 1; i < n; ++i)
        {
            pair<int, int> cur = {c[p[i]], c[(p[i] + (1 << k)) % n]};
            pair<int, int> prev = {c[p[i - 1]], c[(p[i - 1] + (1 << k)) % n]};
            if (cur != prev)
            {
                ++classes;
            }
            cn[p[i]] = classes - 1;
        }
        // swap c and cn
        c.swap(cn);
    }
    return p;
}
vector<int> constructSuffixArray(vector<int> &arr)
{
    arr.push_back(-1); // smallest number (since all arr numbers >= 0), so guaranteed to end up at front
    vector<int> sortedShifts = sortCyclicShifts(arr);
    arr.pop_back();                           // undo change to arr
    sortedShifts.erase(sortedShifts.begin()); // get rid of the thing associated with our "-1"; O(n) operation
    return sortedShifts;
}
vector<int> constructLCPArray(vector<int> &arr, vector<int> &suffixArray)
{
    int n = arr.size();
    int h = 0;
    vector<int> rank(n), lcp(n);

    for (int i = 0; i < suffixArray.size(); ++i)
    {
        rank[suffixArray[i]] = i;
    }

    for (int i = 0; i < n; ++i)
    {
        if (rank[i] > 0)
        {
            int j = suffixArray[rank[i] - 1];
            while (i + h < n && j + h < n && arr[i + h] == arr[j + h])
            {
                ++h;
            }
            lcp[rank[i]] = h;
            if (h > 0)
            {
                h -= 1;
            }
        }
    }
    return lcp;
}
// better checker
int calculateResultingLength(vector<int> &tokens, map<int, TokenInfo> &reverseDistinctTokens)
{
    if (tokens.size() == 0)
    {
        return 0;
    }
    int length = reverseDistinctTokens[tokens[0]].weight;
    for (int i = 1; i < tokens.size(); ++i)
    {
        TokenInfo prev = reverseDistinctTokens[tokens[i - 1]];
        TokenInfo cur = reverseDistinctTokens[tokens[i]];
        if (!prev.isPP && !cur.isPP && !prev.isPunctuator && !cur.isPunctuator)
        {
            // !prev.isPP && !cur.isPP && !prev.isPunctuator && !cur.isPunctuator)
            // means that we need a space between prev and cur
            length += 1;
        }
        // and add cur's weight too
        length += cur.weight;
    }
    return length;
}
vector<int> replaceOccurrences(vector<int> &source, vector<int> &part, int replacement)
{
    // compute pi for part
    vector<int> pi(part.size(), 0);
    for (int i = 1; i < part.size(); ++i)
    {
        int length = pi[i - 1];
        while (length > 0 && part[i] != part[length])
        {
            length = pi[length - 1];
        }
        if (part[i] == part[length])
        {
            ++length;
        }
        pi[i] = length;
    }
    // now use that to match through source
    vector<int> result;
    int length = 0;
    for (int i = 0; i < source.size(); ++i)
    {
        while (length > 0 && source[i] != part[length])
        {
            length = pi[length - 1];
        }
        if (source[i] == part[length])
        {
            ++length;
        }

        // check if it was a match
        result.push_back(source[i]);
        if (length == part.size())
        {
            // pop off part
            for (int j = 0; j < part.size(); ++j)
            {
                result.pop_back();
            }
            length = 0;                    // reset to prevent overlap matches
            result.push_back(replacement); // and push replacement
        }
    }
    return result;
}

pair<int, vector<int>> mostValuableSubarrayV2(vector<int> &tokens, map<int, TokenInfo> &reverseDistinctTokens, int replacement, bool niceMacros)
{
    int n = tokens.size();
    vector<int> suffixArray = constructSuffixArray(tokens);
    vector<int> lcpArray = constructLCPArray(tokens, suffixArray);

    int minLength = numeric_limits<int>::max();
    vector<int> best;
    for (int i = 1; i < n; ++i)
    {
        int length = lcpArray[i];
        if (length == 0)
        {
            continue;
        }

        // collect part, checking for parentheses, brackets, and braces
        int start = suffixArray[i];
        vector<int> part;
        vector<bool> goodIncluding; // false after first negative
        int parenCount = 0, bracketCount = 0, braceCount = 0;
        bool matched = true;
        for (int j = 0; j < length; ++j)
        {
            part.push_back(tokens[start + j]);

            // match checking
            if (reverseDistinctTokens[tokens[start + j]].spelling == "(")
                ++parenCount;
            else if (reverseDistinctTokens[tokens[start + j]].spelling == ")")
                --parenCount;
            else if (reverseDistinctTokens[tokens[start + j]].spelling == "[")
                ++bracketCount;
            else if (reverseDistinctTokens[tokens[start + j]].spelling == "]")
                --bracketCount;
            else if (reverseDistinctTokens[tokens[start + j]].spelling == "{")
                ++braceCount;
            else if (reverseDistinctTokens[tokens[start + j]].spelling == "}")
                --braceCount;
            if (parenCount < 0 || bracketCount < 0 || braceCount < 0)
                matched = false;
            goodIncluding.push_back(matched);
        }
        matched = matched && (parenCount == 0 && bracketCount == 0 && braceCount == 0);

        // if we want to check for nice macros, check that and potentially skip this match
        if (niceMacros && !matched)
        {
            // so it's not nice
            // but it may be the only one of its kind that we'll check
            // because the property of prefix of suffix means that we might be checking something like
            // printf (
            // and if we don't get rid of the trailing (, then we'll never end up replacing the printf part
            // so thus try to remove trailing parentheses/brackets/braces
            while (part.size() > 0 && (!goodIncluding[part.size() - 1] || !(parenCount == 0 && bracketCount == 0 && braceCount == 0)))
            {
                // pop off the last token
                int num = part.back();
                part.pop_back();
                goodIncluding.pop_back();
                // adjust counts
                if (reverseDistinctTokens[num].spelling == "(")
                    --parenCount;
                else if (reverseDistinctTokens[num].spelling == ")")
                    ++parenCount;
                else if (reverseDistinctTokens[num].spelling == "[")
                    --bracketCount;
                else if (reverseDistinctTokens[num].spelling == "]")
                    ++bracketCount;
                else if (reverseDistinctTokens[num].spelling == "{")
                    --braceCount;
                else if (reverseDistinctTokens[num].spelling == "}")
                    ++braceCount;
            }
            if (part.size() == 0)
            {
                continue; // no valid match
            }
        }

        // calculate length of resulting tokens
        vector<int> resultingTokens = replaceOccurrences(tokens, part, replacement);
        int resultingLength = calculateResultingLength(resultingTokens, reverseDistinctTokens);
        // but also add the length from the define
        // "#define " + replacement + " " + part + "\n"
        resultingLength += DEFINE_WEIGHT + reverseDistinctTokens[replacement].weight + calculateResultingLength(part, reverseDistinctTokens);

        if (resultingLength < minLength)
        {
            minLength = resultingLength;
            best = part;
        }
    }
    return {minLength, best};
}

// process
void AddDefinesAction::ExecuteAction()
{
    // step 1 - lex the file into raw tokens;
    SourceManager &sm = getCompilerInstance().getSourceManager();
    auto [tokens, endLocation] = getTokens(sm);

    // next up, convert that into distinct numbers
    int cur = 0;
    map<TokenInfo, int> distinctTokens;
    map<TokenInfo, int> distinctPPTokens;
    map<int, TokenInfo> reverseDistinctTokens;
    vector<int> tokenNumbers;
    for (TokenInfo &token : tokens)
    {
        // special case for PP tokens and main
        if (token.isPP || token.spelling == "main")
        {
            if (distinctPPTokens.find(token) == distinctPPTokens.end())
            {
                distinctPPTokens[token] = cur++;
                // make it 0 that way later algorithms will never touch this
                token.weight = 0;
                reverseDistinctTokens[distinctPPTokens[token]] = token;
            }
            tokenNumbers.push_back(distinctPPTokens[token]);
        }
        else
        {
            if (distinctTokens.find(token) == distinctTokens.end() && !token.isPP)
            {
                distinctTokens[token] = cur++;
                token.weight = token.spelling.length();
                reverseDistinctTokens[distinctTokens[token]] = token;
            }
            tokenNumbers.push_back(distinctTokens[token]);
        }
    }

    // put the first unused symbol into known tokens
    set<string> reserved; // empty, just for convenience
    int curUnusedSymbol = firstUnusedSymbol;
    auto [nextUnusedSymbol, curString] = toSymbol(curUnusedSymbol, reserved, &reserved);
    TokenInfo curStringToken(curString, false, false);
    distinctTokens[curStringToken] = cur++; // use cur, not curUnusedSymbol since curUnusedSymbol will be different and probably less
    curStringToken.weight = curString.length();
    reverseDistinctTokens[distinctTokens[curStringToken]] = curStringToken;

    // continuously replace the most valuable subarray while it's worth it
    int curLength = calculateResultingLength(tokenNumbers, reverseDistinctTokens);
    auto [length, sequence] = mostValuableSubarrayV2(tokenNumbers, reverseDistinctTokens, distinctTokens[curStringToken], niceMacros);
    vector<string> definesToAdd;
    while (length < curLength)
    {
        // replace all instances of the returned subarray with the replacement token
        vector<int> editedTokenNumbers = replaceOccurrences(tokenNumbers, sequence, distinctTokens[curStringToken]);
        // add the definition at the top of the file
        string defineString = "#define " + curString + " ";
        for (int i = 0; i < sequence.size(); ++i)
        {
            defineString += reverseDistinctTokens[sequence[i]].spelling + " ";
        }
        defineString += "\n";

        // add to distinctTokens
        TokenInfo defineToken(defineString, true, false);
        distinctPPTokens[defineToken] = cur++;
        defineToken.weight = 0; // is a preprocessor
        reverseDistinctTokens[distinctPPTokens[defineToken]] = defineToken;
        definesToAdd.push_back(defineString);

        // update tokenNumbers and curLength
        tokenNumbers = std::move(editedTokenNumbers); // no point in waiting for a copy since we're just gonna discard editedTokenNumbers
        curLength = calculateResultingLength(tokenNumbers, reverseDistinctTokens);
        // now we can compute the next unused symbol
        curUnusedSymbol = nextUnusedSymbol;
        pair<int, string> nextP = toSymbol(curUnusedSymbol, reserved, &reserved);
        nextUnusedSymbol = nextP.first;
        curString = nextP.second;
        curStringToken = TokenInfo(curString, false, false);
        distinctTokens[curStringToken] = cur++;
        curStringToken.weight = curString.length();
        reverseDistinctTokens[distinctTokens[curStringToken]] = curStringToken;
        // and compute the next most valuable subarray
        pair<int, vector<int>> result = mostValuableSubarrayV2(tokenNumbers, reverseDistinctTokens, distinctTokens[curStringToken], niceMacros);
        length = result.first;
        sequence = result.second;
    }

    // convert back into tokens
    string resultString;
    for (string define : definesToAdd)
    {
        resultString += define;
    }
    for (int tokenNumber : tokenNumbers)
    {
        resultString += reverseDistinctTokens[tokenNumber].spelling;
        resultString += " ";
    }
    llvm::cantFail(replacements->add(Replacement(sm, CharSourceRange::getCharRange(sm.getLocForStartOfFile(sm.getMainFileID()), endLocation), resultString)));
}
// adapter
unique_ptr<FrontendActionFactory> AddDefinesAction::newAddDefinesAction(int firstUnusedSymbol, bool niceMacros, Replacements *replacements)
{
    class Adapter : public FrontendActionFactory
    {
    private:
        int firstUnusedSymbol;
        bool niceMacros;
        Replacements *replacements;

    public:
        Adapter(int firstUnusedSymbol, bool niceMacros, Replacements *replacements) : firstUnusedSymbol(firstUnusedSymbol), niceMacros(niceMacros), replacements(replacements) {};
        virtual unique_ptr<FrontendAction> create() override
        {
            return make_unique<AddDefinesAction>(firstUnusedSymbol, niceMacros, replacements);
        }
    };
    return make_unique<Adapter>(firstUnusedSymbol, niceMacros, replacements);
}