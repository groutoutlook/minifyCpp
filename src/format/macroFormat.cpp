#include <util/symbols.hpp>
#include <format/macroFormat.hpp>
#include <algorithm>
#include <sstream>

// namespaces
using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace std;

// ctor
MacroFormatter::MacroFormatter(IntrusiveRefCntPtr<llvm::vfs::FileSystem> fileSystem, const string &mainFileName, int firstUnusedSymbol) : fileSystem(fileSystem), mainFileName(mainFileName), firstUnusedSymbol(firstUnusedSymbol) {}

struct TokenInfo
{
    string spelling;
    bool isPP;

    // ctor
    TokenInfo(string spelling, bool isPP = false) : spelling(spelling), isPP(isPP) {}
    friend ostream &operator<<(ostream &o, const TokenInfo &t) { return o << t.spelling; }
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
        tokens.push_back(TokenInfo(spelling, isPP));
    }

    return {tokens, tok.getLocation()};
}

pair<int, vector<int>> mostValuableSubarrayV1(vector<int> &tokens, map<int, int> &weights, int k)
{
    // brute force O(N**3) solution
    int maxWorth = numeric_limits<int>::min();
    pair<int, int> resultStartEnd; // [start, end] inclusive
    for (int i = 0; i < tokens.size(); ++i)
    {
        int weight = 0;
        vector<int> pi = {0}; // kmp pi array
        for (int j = i; j < tokens.size(); ++j)
        {
            // add this token
            weight += weights[tokens[j]];
            if (j - i > 0)
            {
                // calculate pi
                int length = pi[j - i - 1];
                while (length > 0 && tokens[j] != tokens[i + length])
                {
                    length = pi[length - 1];
                }
                if (tokens[j] == tokens[i + length])
                {
                    length += 1;
                }
                pi.push_back(length);
            }

            // now scan the remainder of the string to get counts
            int counts = 1;
            int length = 0;
            for (int l = j + 1; l < tokens.size(); ++l)
            {
                while (length > 0 && tokens[l] != tokens[i + length])
                {
                    length = pi[length - 1];
                }
                if (tokens[l] == tokens[i + length])
                {
                    length += 1;
                }

                // check if found a match; if so, reset length to avoid overlaps
                if (length == j - i + 1)
                {
                    counts += 1;
                    length = 0;
                }
            }

            // now update maxWorth potentially
            int worth = (counts - 1) * (weight - k);
            if (worth > maxWorth)
            {
                maxWorth = worth;
                resultStartEnd = {i, j};
            }
        }
    }

    // return result
    vector<int> result;
    for (int i = resultStartEnd.first; i <= resultStartEnd.second; ++i)
    {
        result.push_back(tokens[i]);
    }
    return {maxWorth, result};
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
    arr.push_back(-1);
    vector<int> sortedShifts = sortCyclicShifts(arr);
    arr.pop_back();
    sortedShifts.erase(sortedShifts.begin()); // everything except front; O(n) operation
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
int countOccurrences(vector<int> &source, vector<int> &part)
{
    if (part.size() > source.size())
    {
        return 0;
    }
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

    // use that to match through source
    int length = 0;
    int count = 0;
    for (int i = 1; i < source.size(); ++i)
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
        if (length == part.size())
        {
            length = 0; // reset to prevent overlap matches
            count++;
        }
    }
    return count;
}
pair<int, vector<int>> mostValuableSubarrayV2(vector<int> &tokens, map<int, int> &weights, int k)
{
    int n = tokens.size();
    vector<int> suffixArray = constructSuffixArray(tokens);
    vector<int> lcpArray = constructLCPArray(tokens, suffixArray);

    int maxWorth = numeric_limits<int>::min();
    vector<int> best;
    for (int i = 1; i < n; ++i)
    {
        int length = lcpArray[i];
        if (length == 0)
        {
            continue;
        }

        // calculate weight and collect part
        int start = suffixArray[i];
        int weightedSum = 0;
        vector<int> part;
        for (int j = 0; j < length; ++j)
        {
            weightedSum += weights[tokens[start + j]];
            part.push_back(tokens[start + j]);
        }
        // calculate true worth
        int counts = countOccurrences(tokens, part);
        int worth = (counts - 1) * (weightedSum - k);
        if (worth > maxWorth)
        {
            maxWorth = worth;
            best = std::move(part);
        }
    }
    return {maxWorth, best};
}

// process
clang::tooling::Replacements MacroFormatter::process()
{
    // initialize sourcemanager and set main file
    IntrusiveRefCntPtr<clang::FileManager> fileManagerPtr(new FileManager(FileSystemOptions(), fileSystem));
    IntrusiveRefCntPtr<DiagnosticOptions> diagOpts(new DiagnosticOptions());
    DiagnosticsEngine diagnostics(
        IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs), &*diagOpts);
    SourceManager sm(diagnostics, *fileManagerPtr);
    sm.setMainFileID(sm.getOrCreateFileID(*fileManagerPtr->getFileRef(mainFileName), SrcMgr::C_User));

    // initialize result
    clang::tooling::Replacements result;

    // step 1 - lex the file into raw tokens;
    auto [tokens, endLocation] = getTokens(sm);

    // next up, convert that into distinct numbers
    int cur = 0;
    map<string, int> distinctTokens;
    map<string, int> distinctPPTokens;
    map<int, string> reverseDistinctTokens;
    map<int, int> weights; // weight[tokenNumber] = length(token.spelling)
    vector<int> tokenNumbers;
    for (const TokenInfo &token : tokens)
    {
        // special case for PP tokens and main
        if (token.isPP || token.spelling == "main")
        {
            if (distinctPPTokens.find(token.spelling) == distinctPPTokens.end())
            {
                distinctPPTokens[token.spelling] = cur++;
                // make it 0 that way later algorithms will never touch this
                weights[distinctPPTokens[token.spelling]] = 0;
                reverseDistinctTokens[distinctPPTokens[token.spelling]] = token.spelling;
            }
            tokenNumbers.push_back(distinctPPTokens[token.spelling]);
        }
        else
        {
            if (distinctTokens.find(token.spelling) == distinctTokens.end() && !token.isPP)
            {
                distinctTokens[token.spelling] = cur++;
                weights[distinctTokens[token.spelling]] = token.spelling.length();
                reverseDistinctTokens[distinctTokens[token.spelling]] = token.spelling;
            }
            tokenNumbers.push_back(distinctTokens[token.spelling]);
        }
    }

    // put the first unused symbol into known tokens
    set<string> reserved; // empty, just for convenience
    int curUnusedSymbol = firstUnusedSymbol;
    auto [nextUnusedSymbol, curString] = toSymbol(curUnusedSymbol, reserved, &reserved);
    distinctTokens[curString] = cur++; // use cur, not curUnusedSymbol since curUnusedSymbol will be different and probably less
    weights[distinctTokens[curString]] = curString.length();
    reverseDistinctTokens[distinctTokens[curString]] = curString;

    // continuously replace the most valuable subarray while it's worth it
    auto [worth, sequence] = mostValuableSubarrayV2(tokenNumbers, weights, curString.length());
    // TODO - take into account the fact that this is an identifier, so replacing
    // punctuation will actually result in an extra space
    while (worth > 11) // 11 because have to define it and add the newlines
    {
        // replace all instances of the returned subarray
        // with a single token that corresponds to curString

        // to do that, let's use smth similar to kmp
        // precompute pi for the sequence
        vector<int> pi(sequence.size(), 0);
        for (int i = 1; i < sequence.size(); ++i)
        {
            int length = pi[i - 1];
            while (length > 0 && sequence[i] != sequence[length])
            {
                length = pi[length - 1];
            }
            if (sequence[i] == sequence[length])
            {
                ++length;
            }
            pi[i] = length;
        }

        // add the definition at the top of the file
        vector<int> editedTokenNumbers;
        string defineString = "\n#define " + curString + " ";
        for (int i = 0; i < sequence.size(); ++i)
        {
            defineString += reverseDistinctTokens[sequence[i]] + " ";
        }
        defineString += "\n";
        // add to distinctTokens
        distinctPPTokens[defineString] = cur++;
        weights[distinctPPTokens[defineString]] = 0; // is a preprocessor
        reverseDistinctTokens[distinctPPTokens[defineString]] = defineString;
        editedTokenNumbers.push_back(distinctPPTokens[defineString]);

        // then use pi to find all matches in tokenNumbers
        int length = 0;
        for (int i = 0; i < tokenNumbers.size(); ++i)
        {
            while (length > 0 && tokenNumbers[i] != sequence[length])
            {
                length = pi[length - 1];
            }
            if (sequence[length] == tokenNumbers[i])
            {
                ++length;
            }
            editedTokenNumbers.push_back(tokenNumbers[i]);
            // if we found a match, replace it with the new symbol
            if (length == sequence.size())
            {
                length = 0; // reset that way we don't get overlaps
                for (int j = 0; j < sequence.size(); ++j)
                {
                    editedTokenNumbers.pop_back();
                }
                editedTokenNumbers.push_back(distinctTokens[curString]);
            }
        }

        // update tokenNumbers
        tokenNumbers = std::move(editedTokenNumbers); // no point in waiting for a copy since we're just gonna discard editedTokenNumbers
        // now we can compute the next unused symbol
        curUnusedSymbol = nextUnusedSymbol;
        pair<int, string> nextP = toSymbol(curUnusedSymbol, reserved, &reserved);
        nextUnusedSymbol = nextP.first;
        curString = nextP.second;
        distinctTokens[curString] = cur++;
        weights[distinctTokens[curString]] = curString.length();
        reverseDistinctTokens[distinctTokens[curString]] = curString;
        // and compute the next most valuable subarray
        pair<int, vector<int>> result = mostValuableSubarrayV2(tokenNumbers, weights, curString.length());
        worth = result.first;
        sequence = result.second;
    }

    // convert back into tokens
    string resultString;
    for (int tokenNumber : tokenNumbers)
    {
        resultString += reverseDistinctTokens[tokenNumber];
        resultString += " ";
    }
    Replacements replacements;
    llvm::cantFail(replacements.add(Replacement(sm, CharSourceRange::getCharRange(sm.getLocForStartOfFile(sm.getMainFileID()), endLocation), resultString)));
    return replacements;
}