#include "StringUtils.hpp"

namespace crisp
{
    std::vector<std::string> tokenize(const std::string& string, const std::string& delimiter)
    {
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = 0;

        while (end != std::string::npos)
        {
            end = string.find(delimiter, start);

            result.push_back(string.substr(start, (end == std::string::npos) ? std::string::npos : end - start));

            start = ((end > (std::string::npos - delimiter.size())) ? std::string::npos : end + delimiter.size());
        }

        return result;
    }
}
