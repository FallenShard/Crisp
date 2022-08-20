#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace crisp
{
    std::vector<std::string> tokenize(std::string_view string, const std::string& delimiter);
    std::vector<std::string_view> tokenizeIntoViews(std::string_view string, std::string_view delimiter);

    template <std::size_t TokenCount>
    std::array<std::string_view, TokenCount> fixedTokenize(std::string_view string, std::string_view delimiter, bool skipEmptyTokens = true)
    {
        std::array<std::string_view, TokenCount> result;
        size_t start = 0;
        size_t end = 0;

        size_t tokenIdx = 0;

        while (end != std::string::npos && tokenIdx < TokenCount)
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
}  // namespace crisp