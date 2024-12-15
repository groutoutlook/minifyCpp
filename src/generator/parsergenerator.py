from generator.types import *
import os


def generate_parser(grammar: Grammar) -> str:
    out = [open(os.path.join(os.path.dirname(__file__), "parserstub.py")).read()]
    out.append(f"grammar = {grammar}")
    out.append(f"parser = Parser(grammar)")
    return "\n".join(out)
