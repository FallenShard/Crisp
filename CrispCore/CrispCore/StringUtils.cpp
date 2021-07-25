#include "StringUtils.hpp"

namespace crisp
{
    std::vector<std::string> tokenize(const std::string_view string, const std::string& delimiter)
    {
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = 0;

        while (end != std::string::npos)
        {
            end = string.find(delimiter, start);

            const std::string_view tokenView = string.substr(start, (end == std::string::npos) ? std::string::npos : end - start);
            result.emplace_back(tokenView);

            start = end + delimiter.size();
        }

        return result;
    }
}
