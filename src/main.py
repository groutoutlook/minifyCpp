from generator.lexer import Lexer
from out import parser
from lang.macrotokens import tokens, regexes, t_ignore

if __name__ == "__main__":
    inp = open("tmp.c").read()
    l = Lexer(tokens, regexes, t_ignore)
    ts = l.lex(inp)
    print(ts)
    parsed = parser.parse("translation_unit", ts)
    print(parsed)
