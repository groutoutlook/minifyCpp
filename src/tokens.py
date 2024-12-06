import ply.lex as lex
import sys

# see https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf
# starting from page 61
tokens = [
    # important things
    "KEYWORD",
    "IDENTIFIER",
    # chose not to implement universal character names
    "CONSTANT",
    "STRINGLITERAL",
    "PUNCTUATOR",
    "HEADERNAME",
    "COMMENT",
]

t_KEYWORD = r"|".join(
    map(
        lambda a: rf"({a})",
        sorted(
            [
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
            ],
            key=len,
            reverse=True,
        ),
    )
)
t_IDENTIFIER = r"[a-zA-Z_][a-zA-Z0-9_]*"

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
t_PUNCTUATOR = r"|".join(
    map(
        lambda a: rf"({a})",
        sorted(
            [
                r"\[",
                r"\]",
                r"\(",
                r"\)",
                r"\{",
                r"\}",
                r"\.",
                r"\->",
                r"\+\+",
                r"\-\-",
                r"&",
                r"\*",
                r"\+",
                r"\-",
                r"\~",
                r"\!",
                r"/",
                # r"%",
                r"<<",
                r">>",
                r"<",
                r">",
                r"<=",
                r">=",
                r"==",
                r"!=",
                r"\^",
                r"\|",
                r"&&",
                r"\|\|",
                r"\?",
                r":",
                r";",
                r"\.\.\.",
                r"=",
                r"\*=",
                r"/=",
                r"%=",
                r"\+=",
                r"\-=",
                r"<<=",
                r">>=",
                r"&=",
                r"\^=",
                r"\|=",
                r",",
                r"\#",
                r"\#\#",
                r"<:",
                r":>",
                r"<%",
                r"%>",
                r"%:",
                r"%:%:",
            ],
            key=len,
            reverse=True,
        ),
    )
)

t_ignore = " \t\n"


def t_COMMENT(t):
    r"(/\*(.*?\n?)*?\*/)|(//.*)"


@lex.TOKEN(headername)
def t_HEADERNAME(t):
    return t


if __name__ == "__main__":
    inp = sys.stdin.read()
    lexer = lex.lex()
    lexer.input(inp)
    a = lexer.token()
    while a:
        print(a, a.value)
        a = lexer.token()
