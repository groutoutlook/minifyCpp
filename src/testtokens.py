import generator.lexer as lexer
from testparser import parser

int_regex = r"[0-9]+"
symbols = r"(\+\+\+)|(\-\-\-)|(\*\*\*)"

if __name__ == "__main__":
    inp = open("src/expr.txt").read()
    # import sys

    # inp = sys.stdin.read()
    l = lexer.Lexer(["INT", "symbols"], [int_regex, symbols], r"\t\n ")
    ts = l.lex(inp)
    out = parser.parse("expr", ts)
    print(out)
