from generator.lexer import Lexer
from lang.cparser import parser
from lang.minify import format
from lang.macrotokens import tokens, regexes, t_ignore

if __name__ == "__main__":
    inp = open("tmp.c").read()
    l = Lexer(tokens, regexes, t_ignore)
    ts = l.lex(inp)
    # print(ts)
    # parse and then apply minification rules
    # to variable names
    # TODO - use the parsed stuff
    parsed = parser.parse("translation_unit", ts)
    # print(parsed)

    # minify by removing spaces
    minified = format(ts)  # TODO - use the parsed stuff instead
    print(minified)
