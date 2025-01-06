from typing import List, Dict
from generator.lexer import Token
from generator.ast import ASTNode
import logging


logger = logging.getLogger(__name__)


def to_symbol(i: int) -> str:
    result = []
    while i >= 0:
        tmp = i % 52  # use every lowercase and uppercase letter
        if tmp < 26:
            result.append(chr(ord("a") + tmp))
        else:
            result.append(chr(ord("A") + tmp - 26))
        i //= 52
        i -= 1
    return "".join(reversed(result))


def dfs(cur: ASTNode, cur_max_symbol: int, symbol_map: Dict[str, str]):
    """
    Replace variables with a dfs. Variables will be replaced by assuming
    that variables should be visible to siblings and descendants, but not
    ancestors of the current Node

    Args:
        cur (ASTNode): the current node
        cur_max_symbol (int): the current max symbol, exclusive
        symbol_map (Dict[str, str]): the symbol map

    Returns:
        result (Tuple[int, int]): a tuple containing (cur_max_symbol, max_symbol)
            where cur_max_symbol is the current max symbol, and max_symbol is the max symbol

    Raises:
        ValueError: if a variable is not defined

    Note:
        The symbol_map is modified to include new variables
    """

    # if compound statement, then the compound statement's siblings shouldn't be able to access
    # anything from inside the compound statement
    # for loops also have a special case since the variable declared as part 1 of the for loop
    # should only be accessible inside the for loop and not outside

    # everything else follows the below rule:
    # any variable declared inside it will be accessible to its siblings and descendants
    # and passed up to ancestors
    max_symbol = cur_max_symbol
    # declaration (direct declarator)
    if cur.rule == "direct_declarator":
        # this adds variables to the symbol map
        for part in cur.parts:
            for leaf in part.leaves:
                if isinstance(leaf, Token):
                    if leaf.type == "IDENTIFIER":
                        if leaf.value not in symbol_map:
                            # main is a special case
                            if leaf.value == "main":
                                symbol_map[leaf.value] = "main"
                            else:
                                symbol_map[leaf.value] = to_symbol(cur_max_symbol)
                                leaf.value = to_symbol(cur_max_symbol)
                                cur_max_symbol += 1
                                max_symbol = max(max_symbol, cur_max_symbol)
                        else:
                            # shouldn't happen?
                            raise ValueError(
                                f"Variable {leaf.value} is already defined"
                            )
                else:
                    # dfs further
                    cur_max_symbol, max_symbol = dfs(leaf, cur_max_symbol, symbol_map)
        return cur_max_symbol, max_symbol
    else:
        result_symbol_map = symbol_map
        original_cur_max_symbol = cur_max_symbol
        # compound statement case and for loop case
        special = cur.rule == "compound_statement" or cur.rule == "iteration_statement"
        if special:
            result_symbol_map = symbol_map.copy()
        for part in cur.parts:
            for leaf in part.leaves:
                if isinstance(leaf, Token):
                    if leaf.type == "IDENTIFIER":
                        if leaf.value not in symbol_map:
                            logger.warning(
                                f"Variable {leaf.value} is not defined in the current scope"
                            )
                        else:
                            leaf.value = symbol_map[leaf.value]
                else:
                    # dfs further
                    cur_max_symbol, max_symbol = dfs(leaf, cur_max_symbol, symbol_map)
        if special:
            # TODO: make this more efficient
            symbol_map.clear()
            symbol_map.update(result_symbol_map)
            return original_cur_max_symbol, max_symbol
        return cur_max_symbol, max_symbol


def minify(root: ASTNode):
    dfs(root, 0, {})

    return root


def extract_tokens(root: ASTNode) -> List[Token]:
    result = []
    for part in root.parts:
        for leaf in part.leaves:
            if isinstance(leaf, Token):
                result.append(leaf)
            else:
                result.extend(extract_tokens(leaf))
    return result


def format(tokens: List[Token]) -> str:
    result: List[str] = []
    for i in range(len(tokens)):
        # these get special case
        if tokens[i].type == "PREPROCESSOR":
            result.append(tokens[i].value)
            result.append("\n")
        # otherwise, put spaces between everything unless it's a punctuator
        elif tokens[i].type == "PUNCTUATOR":
            if len(result) > 0 and result[-1] == " ":
                result.pop()
            result.append(tokens[i].value)
        else:
            result.append(tokens[i].value)
            result.append(" ")
    return "".join(result).strip()
