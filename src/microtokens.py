"""
File: microtokens.py

This file contains the lexer for the C language, implemented
according to see https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf

This file provides the following:
    lex(inp: str) -> List[l.LexToken]
        This function lexes a file, returning all the lexical tokens
        found in the file, or erroring if a lexical error exists
    tokens: List[str]
        This contains the names of all the lexical tokens

"""

from typing import List
import ply.lex as l
import sys

# see https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf
# starting from page 61
tokens = [
    # important things
    "IDENTIFIER",
    # chose not to implement universal character names
    "CONSTANT",
    "STRINGLITERAL",
    "HEADERNAME",
    "COMMENT",
    # punctuators
    # "SEMICOLON",
    # "COLON",
    # "BITWISEOR",
    "ARROW",
    "PLUSPLUS",
    "MINUSMINUS",
    "LSHIFT",
    "RSHIFT",
    "LEQ",
    "GEQ",
    "EQEQ",
    "NEQ",
    "LAND",
    "LOR",
    "TIMESEQ",
    "DIVEQ",
    "MODEQ",
    "PLUSEQ",
    "MINUSEQ",
    "LSHIFTEQ",
    "RSHIFTEQ",
    "ANDEQ",
    "XOREQ",
    "OREQ",
    "DOTDOTDOT",
    "POUNDPOUND",
    # keywords
    "SIZEOF",
    "AUTO",
    "BREAK",
    "CASE",
    "CHAR",
    "CONST",
    "CONTINUE",
    "DEFAULT",
    "DO",
    "DOUBLE",
    "ELSE",
    "ENUM",
    "EXTERN",
    "FLOAT",
    "FOR",
    "GOTO",
    "IF",
    "INLINE",
    "INT",
    "LONG",
    "REGISTER",
    "RESTRICT",
    "RETURN",
    "SHORT",
    "SIGNED",
    "STATIC",
    "STRUCT",
    "SWITCH",
    "TYPEDEF",
    "UNION",
    "UNSIGNED",
    "VOID",
    "VOLATILE",
    "WHILE",
    "UBOOL",
    "UCOMPLEX",
    "UIMAGINARY",
]
keywords = {
    # keywords
    "sizeof": "SIZEOF",
    "auto": "AUTO",
    "breal": "BREAK",
    "case": "CASE",
    "char": "CHAR",
    "const": "CONST",
    "continue": "CONTINUE",
    "default": "DEFAULT",
    "do": "DO",
    "double": "DOUBLE",
    "else": "ELSE",
    "enum": "ENUM",
    "extern": "EXTERN",
    "float": "FLOAT",
    "for": "FOR",
    "goto": "GOTO",
    "if": "IF",
    "inline": "INLINE",
    "int": "INT",
    "long": "LONG",
    "register": "REGISTER",
    "restrict": "RESTRICT",
    "return": "RETURN",
    "short": "SHORT",
    "signed": "SIGNED",
    "static": "STATIC",
    "struct": "STRUCT",
    "switch": "SWITCH",
    "typedef": "TYPEDEF",
    "union": "UNION",
    "unsigned": "UNSIGNED",
    "void": "VOID",
    "volatile": "VOLATILE",
    "while": "WHILE",
    "_Bool": "UBOOL",
    "_Complex": "UCOMPLEX",
    "_Imaginary": "UIMAGINARY",
}


# identifiers
def t_IDENTIFIER(t):
    r"[a-zA-Z_][a-zA-Z0-9_]*"
    if t.value in keywords:
        t.type = keywords[t.value]
    return t


# (hex | decimal | octal) (suffix)
integer_constant = r"((0[xX][0-9a-fA-F]+)|([1-9][0-9]*)|([0-7]+))(u|l|U|L|(ll)|(LL))?"
# (decimal floating constant | hex floating constant) [suffix]
dec_floating_constant = r"(([0-9]+(\.[0-9]*)?)|(\.[0-9]+))([eE][\+\-]?[0-9]+)?"
hex_floating_constant = r"(0[xX](([0-9a-fA-F]+(\.[0-9a-fA-F]*)?))|([0-9a-fA-F]*\.[0-9a-fA-F]+))[pP][\+\-]?[0-9]+"
floating_constant = rf"({hex_floating_constant})|({dec_floating_constant})[flFL]?"

simple_escape_sequence = r"\\['\"\?\\abfnrtv]"
octal_escape_sequence = r"\\[0-7]{1,3}"
hex_escape_sequence = r"\\x[0-9a-fA-F]+"
charset = r"\!\#$%&\(\)\*\+,\-\.\/0-9:;<=\?\@A-Z\[\]\^_`a-z\{\}\|\~ "
ccharset = rf"[{charset}\">]"
scharset = rf"[{charset}'>]"
hcharset = rf"[{charset}'\"]"
qcharset = rf"[{charset}'>]"
cchar = rf"({simple_escape_sequence})|({octal_escape_sequence})|({hex_escape_sequence})|({ccharset})"
schar = rf"({simple_escape_sequence})|({octal_escape_sequence})|({hex_escape_sequence})|({scharset})"
hchar = rf"({simple_escape_sequence})|({octal_escape_sequence})|({hex_escape_sequence})|({hcharset})"
qchar = rf"({simple_escape_sequence})|({octal_escape_sequence})|({hex_escape_sequence})|({qcharset})"
char_constant = rf"[L]?'({cchar})+'"
t_CONSTANT = rf"({floating_constant})|({integer_constant})|({char_constant})"
t_STRINGLITERAL = rf"[L]?\"({schar})*\""
headername = rf"(<({hchar})+>)|(\"({qchar})+\")"

# punctuators and literals
literals = "()[]{}.&*+-~!/%<>^?=,#;:|"
# t_SEMICOLON = r";"  # needed because ; is reserved in yacc
# t_COLON = r":"  # needed because : is reserved in yacc
# t_BITWISEOR = r"\|"  # needed because | is reserved in yacc
t_ARROW = r"\->"
t_PLUSPLUS = r"\+\+"
t_MINUSMINUS = r"\-\-"
t_LSHIFT = r"<<"
t_RSHIFT = r">>"
t_LEQ = r"<="
t_GEQ = r">="
t_EQEQ = r"=="
t_NEQ = r"!="
t_LAND = r"&&"
t_LOR = r"\|\|"
t_TIMESEQ = r"\*="
t_DIVEQ = r"/="
t_MODEQ = r"%="
t_PLUSEQ = r"\+="
t_MINUSEQ = r"\-="
t_LSHIFTEQ = r"<<="
t_RSHIFTEQ = r">>="
t_ANDEQ = r"&="
t_XOREQ = r"\^="
t_OREQ = r"\|="
t_DOTDOTDOT = r"\.\.\."
t_POUNDPOUND = r"\#\#"

t_ignore = " \t\n"


def t_COMMENT(t):
    r"(/\*(.*?\n?)*?\*/)|(//.*)"


@l.TOKEN(headername)
def t_HEADERNAME(t):
    return t


# for export so that everything is compatable
# with PLY stuff
lexer = l.lex()


def lex(inp: str) -> List[l.LexToken]:
    lexer = l.lex()
    lexer.input(inp)
    ts = []
    a = lexer.token()
    while a:
        ts.append(a)
        a = lexer.token()
    return ts


if __name__ == "__main__":
    inp = sys.stdin.read()
    print(lex(inp))
