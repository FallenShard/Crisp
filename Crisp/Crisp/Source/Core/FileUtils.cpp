#include "FileUtils.hpp"

#define NOMINMAX
#include <Windows.h>

namespace crisp
{
    std::vector<std::string> FileUtils::enumerateFiles(const std::string& directory, const std::string& extension)
    {
        std::vector<std::string> fileNames;

        HANDLE findHandle;
        WIN32_FIND_DATA findData;

        auto searchPattern = directory + "*." + extension;

        findHandle = FindFirstFile(searchPattern.c_str(), &findData);
        if (findHandle != INVALID_HANDLE_VALUE)
        {
            fileNames.push_back(findData.cFileName);
            while (FindNextFile(findHandle, &findData))
            {
                fileNames.push_back(findData.cFileName);
            }
        }

        FindClose(findHandle);

        return fileNames;
    }
}