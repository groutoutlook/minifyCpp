from generator.lexer import Lexer

lexer = Lexer(
    ["STRING", "INT", "LITERAL"],
    regexes=[r"[a-z]+", "[0-9]+", r"\+\-\{\}\(\)\=<"],
    ignore=" \t\n",
)

lexer.lex()