#pragma once
#include <string>
#include <set>
#include <clang/Lex/Token.h>

std::pair<int, std::string> toSymbol(int i, const std::set<std::string> &reserved, std::set<std::string> *defines);
bool isPunctuator(const clang::Token &t);