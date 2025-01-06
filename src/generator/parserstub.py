from generator.types import *
from generator.lexer import Token
from generator.ast import *
from typing import List, Tuple, Any, Dict, Callable, Optional
from dataclasses import dataclass


@dataclass
class ParseResult[T]:
    val: T = None
    pos: int = None
    valid: bool = True

    def __bool__(self):
        return self.valid


class Parser:
    def __init__(
        self,
        grammar: Grammar,
        callbacks: Optional[Dict[str, Callable[[List[Any]], Any]]] = None,
    ):
        self.callbacks = callbacks
        self.rule_map = {rule.name: rule for rule in grammar.rules}
        self.cache: Dict[Tuple[str, int], ParseResult[ASTNode] | ParseResult[None]] = {}
        self.max_pos = 0

    def update(f):
        def wrapper(self, *args, **kwargs):
            result = f(self, *args, **kwargs)
            if result and result.pos > self.max_pos:
                self.max_pos = result.pos
            return result

        return wrapper

    @update
    def __take_leaf(
        self, l: Leaf, tokens: List[Token], pos: int
    ) -> ParseResult[ASTLeaf] | ParseResult[None]:
        if pos >= len(tokens):
            return ParseResult(valid=False)
        if l.type == "LITERAL":
            # in the case of literals, l.value
            # contains the exact literal to match
            if tokens[pos].value != l.value:
                return ParseResult(valid=False)
            return ParseResult(tokens[pos], pos + 1)
        elif l.type == "TERMINAL":
            # in the case of terminals, l.value
            # contains the type of the nonterminal
            if tokens[pos].type != l.value:
                return ParseResult(valid=False)
            return ParseResult(tokens[pos], pos + 1)
        else:
            # in the case of nonterminals, l.value
            # contains the name of the nonterminal
            return self.__check_rule(l.value, tokens, pos)

    def __take_leaves(self, l: List[Leaf], tokens: List[Token], pos: int):
        if pos >= len(tokens):
            return ParseResult(valid=False)
        result = []
        for leaf in l:
            r = self.__take_leaf(leaf, tokens, pos)
            if not r:
                return ParseResult(valid=False)
            res, pos = r.val, r.pos
            result.append(res)
        return ParseResult(ASTPart(result), pos)

    def __check_production(
        self, rule: str, prod: Production, tokens: List[Token], pos: int
    ):
        result = []
        for part in prod.items:
            if part.optional:
                # try taking, but it's ok if it fails
                r = self.__take_leaves(part.items, tokens, pos)
                if r:
                    # it actually succeeded, go with this
                    res, pos = r.val, r.pos
                    result.append(res)
            elif part.star:
                # try taking any amount of this
                r = self.__take_leaves(part.items, tokens, pos)
                while r:
                    res, pos = r.val, r.pos
                    result.append(res)
                    r = self.__take_leaves(part.items, tokens, pos)
            else:
                # must take this item
                r = self.__take_leaves(part.items, tokens, pos)
                if not r:
                    return ParseResult(valid=False)
                res, pos = r.val, r.pos
                result.append(res)
        return ParseResult(ASTNode(rule, result), pos)

    def __check_rule(self, rule: str, tokens: List[Token], pos: int):
        # takes the rule that succeeds that takes the most tokens
        if (rule, pos) in self.cache:
            return self.cache[(rule, pos)]

        actual_rule = self.rule_map[rule]
        best = ParseResult(valid=False)
        for prod in actual_rule.productions:
            r = self.__check_production(rule, prod, tokens, pos)
            if r and (not best.valid or best.pos < r.pos):
                best = r

        self.cache[(rule, pos)] = best
        if best and self.callbacks and rule in self.callbacks:
            self.callbacks[rule](best.val)
        return best

    def parse(self, start_symbol: str, tokens: List[Token]) -> ASTNode:
        self.cache = {}
        result = self.__check_rule(start_symbol, tokens, 0)
        if result and result.pos == len(tokens):
            return result.val
        raise Exception(f"Parse failed, max pos is {self.max_pos}")
