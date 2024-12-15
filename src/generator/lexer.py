from dataclasses import dataclass
import re
from typing import List


@dataclass
class Token:
    type: str
    value: str


class Lexer:
    def __init__(self, names: List[str], regexes: List[str], ignore: str = ""):
        assert len(names) == len(regexes)
        self.names = names
        self.regexes = list(map(re.compile, regexes))
        (*self.ignore,) = ignore
        self.ignore = set(self.ignore)

    def lex(self, contents: str):
        pos = 0
        lineno = 0
        linestart = 0
        tokens = []
        while pos != len(contents):
            # first, check if this is an ignorable token
            if contents[pos] in self.ignore:
                if pos == "\n":
                    lineno += 1
                    linestart = pos + 1
                pos += 1
                continue

            # take token with largest munch
            # and in cases of ties, use
            # the one that came first
            best_token = -1
            best_length = 0
            best_val = ""
            for i in range(len(self.names)):
                m = self.regexes[i].match(contents, pos)
                if m is not None and len(m.group()) > best_length:
                    best_length = len(m.group())
                    best_token = i
                    best_val = m.group()
            if best_token == -1:
                raise Exception(f"Lexing failed at line {lineno} col {pos - linestart}")
            tokens.append(Token(self.names[best_token], best_val))

            # update lineno and linestart
            if "\n" in best_val:
                num_newlines = best_val.count("\n")
                last_newline_pos = best_val.rindex("\n")
                lineno += num_newlines
                linestart = pos + last_newline_pos + 1
            pos += len(best_val)
        return tokens
