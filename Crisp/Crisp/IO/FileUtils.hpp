#pragma once

#include <Crisp/Common/Result.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace crisp
{
std::vector<std::string> enumerateFiles(const std::filesystem::path& directoryPath, const std::string& extension);
Result<std::string> fileToString(const std::filesystem::path& filePath);
[[nodiscard]] Result<> stringToFile(const std::filesystem::path& outputFilePath, const std::string& str);

Result<std::vector<char>> readBinaryFile(const std::filesystem::path& filePath);
std::string openFileDialog();
} // namespace crisp
