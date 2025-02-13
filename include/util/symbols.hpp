#pragma once
#include <string>
#include <set>

std::pair<int, std::string> toSymbol(int i, const std::set<std::string> &reserved, std::set<std::string> *defines);
