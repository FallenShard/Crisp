#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace crisp::fileutils
{
    std::vector<std::string> enumerateFiles(const std::filesystem::path& directoryPath, const std::string& extension);
    std::string fileToString(const std::string& filePath);
    std::vector<char> readBinaryFile(const std::filesystem::path& filePath);
    std::string openFileDialog();
    std::string getFileNameFromPath(const std::string& filePath);
    void createDirectory(const std::string& path);
    bool fileExists(const std::string& filePath);
}
