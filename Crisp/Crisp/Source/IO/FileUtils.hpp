#pragma once

#include <vector>
#include <string>

namespace crisp
{
    class FileUtils
    {
    public:
        static std::vector<std::string> enumerateFiles(const std::string& directory, const std::string& extension);
        static std::string fileToString(const std::string& filePath);
        static std::vector<char> readBinaryFile(const std::string& filePath);
        static std::string openFileDialog();
        static std::string getFileNameFromPath(const std::string& filePath);
        static void createDirectory(const std::string& path);
        static bool fileExists(const std::string& filePath);
    };
}
