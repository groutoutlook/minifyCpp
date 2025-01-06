from dataclasses import dataclass
from typing import List, TypeAlias, Union
from generator.lexer import Token

ASTLeaf: TypeAlias = Union[Token, "ASTNode"]


@dataclass
class ASTPart:
    leaves: List[ASTLeaf]


@dataclass
class ASTNode:
    rule: str
    parts: List[ASTPart]
