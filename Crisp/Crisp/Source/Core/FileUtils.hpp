#pragma once

#include <vector>
#include <string>

namespace crisp
{
    class FileUtils
    {
    public:
        static std::vector<std::string> enumerateFiles(const std::string& directory, const std::string& extension);
    };
}
