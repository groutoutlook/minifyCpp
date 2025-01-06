"""
This file implements an automatic recursive descent
parser generator assuming that all rule rules
are written in docstrings of functions prefixed with p_
and are in EBNF
"""

from generator.grammartokens import *
from generator.types import *
from ply.yacc import yacc
from typing import List


def p_grammar(p: List[str | Rule | Grammar]):
    """
    grammar : rule
        | NEWLINE
        | grammar rule
        | grammar NEWLINE
    """
    if len(p) == 2:
        if isinstance(p[1], str):
            # was a newline, so return an empty grammar
            p[0] = Grammar([])
        else:
            # was a rule
            p[0] = Grammar([p[1]])
    else:
        assert isinstance(p[1], Grammar)
        if isinstance(p[2], str):
            # was a newline, so return an empty grammar
            p[0] = Grammar(p[1].rules)
        else:
            # was a rule
            p[0] = Grammar(p[1].rules + [p[2]])


def p_rule(p: List[Production | Rule | str]):
    """
    rule : NONTERMINAL ':' production NEWLINE
        | rule '|' production NEWLINE
    """
    if isinstance(p[1], str):
        p[0] = Rule(p[1], [p[3]])
    else:
        assert isinstance(p[1], Rule)
        p[1].productions.append(p[3])
        p[0] = p[1]


def p_production(p: List[Production | ProductionPart]):
    """
    production : production_part
        | production production_part
    """
    if len(p) == 2:
        assert isinstance(p[1], ProductionPart)
        p[0] = Production([p[1]])
    else:
        assert isinstance(p[1], Production)
        p[0] = Production(p[1].items + [p[2]])


def p_production_part(p):
    """
    production_part : '[' production_item_list ']'
        | '{' production_item_list '}'
        | production_item_list
    """
    if len(p) == 4:
        if p[1] == "[":
            # optional
            p[0] = ProductionPart(True, False, p[2])
        else:
            # star (any amount of this)
            p[0] = ProductionPart(False, True, p[2])
    else:
        p[0] = ProductionPart(False, False, p[1])


def p_production_item_list(p):
    """
    production_item_list : production_leaf
        | production_item_list production_leaf
    """
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[1].append(p[2])
        p[0] = p[1]


def p_production_leaf(p):
    """
    production_leaf : TERMINAL
        | NONTERMINAL
        | LITERAL
    """
    t = get_type(p[1])
    contents = p[1]
    if t == "LITERAL":
        contents = contents.strip("'")
    p[0] = Leaf(t, contents)


def parse(contents: str) -> Grammar:
    parser = yacc(debug=False)
    return parser.parse(contents, lexer=lexer)
