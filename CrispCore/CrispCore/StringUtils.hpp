#pragma once

#include <string>
#include <string_view>
#include <array>
#include <vector>

namespace crisp
{
    std::vector<std::string> tokenize(const std::string_view string, const std::string& delimiter);

    template <std::size_t TokenCount>
    std::array<std::string_view, TokenCount> fixedTokenize(const std::string_view string, const std::string_view delimiter, bool skipEmptyTokens = true)
    {
        std::array<std::string_view, TokenCount> result;
        size_t start = 0;
        size_t end = 0;

        size_t tokenIdx = 0;

        while (start < string.size() && end != std::string::npos && tokenIdx < TokenCount)
        {
            end = string.find(delimiter, start);

            if (!skipEmptyTokens || end != start)
            {
                result[tokenIdx++] = string.substr(start, end - start);
            }

            start = end + delimiter.size();
        }

        return result;
    }
}