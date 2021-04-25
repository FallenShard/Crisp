#pragma once

#include <string>
#include <string_view>
#include <array>
#include <vector>

namespace crisp
{
    std::vector<std::string> tokenize(const std::string& string, const std::string& delimiter);

    template <std::size_t TokenCount>
    std::array<std::string_view, TokenCount> fixedTokenize(const std::string_view string, const std::string_view delimiter, bool skipEmptyTokens = true)
    {
        std::array<std::string_view, TokenCount> result;
        size_t start = 0;
        size_t end = 0;

        size_t tokenIdx = 0;

        while (start < string.size())
        {
            end = string.find(delimiter, start);

            if (tokenIdx < TokenCount)
            {
                if (skipEmptyTokens)
                {
                    if (end != start)
                        result[tokenIdx++] = string.substr(start, end - start);
                }
                else
                {
                    result[tokenIdx++] = string.substr(start, end - start);
                }
            }
            else
                break;

            if (end == std::string_view::npos)
                break;

            start = end + 1;
        }

        return result;
    }
}