from generator.lexer import Lexer
from lang.cparser import parser
from lang.minify import format, minify, extract_tokens
from lang.macrotokens import tokens, regexes, t_ignore
import argparse

argparser = argparse.ArgumentParser()
argparser.add_argument("file", type=str)


if __name__ == "__main__":
    args = argparser.parse_args()
    inp = open(args.file, "r").read()
    l = Lexer(tokens, regexes, t_ignore)
    ts = l.lex(inp)
    # parse and then apply minification rules
    # to variable names
    parsed = parser.parse("translation_unit", ts)
    parsed = minify(parsed)
    ts = extract_tokens(parsed)

    # minify by removing spaces
    minified = format(ts)
    print(minified)
