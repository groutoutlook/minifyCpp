from typing import List
from generator.lexer import Token


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
