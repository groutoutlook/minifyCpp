from typing import Literal, List
from dataclasses import dataclass


@dataclass
class Leaf:
    type: Literal["TERMINAL", "LITERAL", "NONTERMINAL"]
    value: str


@dataclass
class ProductionPart:
    optional: bool
    star: bool
    items: List[Leaf]


@dataclass
class Production:
    items: List[ProductionPart]


@dataclass
class Rule:
    name: str
    productions: List[Production]


@dataclass
class Grammar:
    rules: List[Rule]
