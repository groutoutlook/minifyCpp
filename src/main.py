import importlib

if __name__ == "__main__":
    module = importlib.import_module("lang.parser")
    print(module.__dict__)
    import impl.grammarparser

    impl.grammarparser.collect_productions()
