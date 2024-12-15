from generator.grammarparser import parse
from generator.parsergenerator import generate_parser

import sys

if __name__ == "__main__":
    grammar = parse(sys.stdin.read())
    parser_code = generate_parser(grammar)
    print(parser_code)
