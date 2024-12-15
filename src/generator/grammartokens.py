from ply.lex import lex
import re

tokens = ["TERMINAL", "LITERAL", "NONTERMINAL", "NEWLINE"]
literals = ":|[]{}"

t_TERMINAL = r"[A-Z_]+"
t_LITERAL = r"'[^']+'"
t_NONTERMINAL = r"[a-z_]+"
t_NEWLINE = r"\n"
t_ignore = " \t"

lexer = lex()


def is_terminal(s: str):
    return re.match(t_TERMINAL, s) is not None


def is_literal(s: str):
    return re.match(t_LITERAL, s) is not None


def is_nonterminal(s: str):
    return re.match(t_NONTERMINAL, s) is not None


def get_type(s: str) -> str:
    fns = [is_terminal, is_literal, is_nonterminal]
    for i in range(len(tokens) - 1):
        if fns[i](s):
            return tokens[i]
    raise Exception(f"String {s} doesn't match any categories")
