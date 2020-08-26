#pragma once

#include <string>
#include <vector>

namespace crisp
{
    std::vector<std::string> tokenize(const std::string& string, const std::string& delimiter);
}