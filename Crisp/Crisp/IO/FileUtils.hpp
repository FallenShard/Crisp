#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <Crisp/Core/Result.hpp>

namespace crisp {
std::vector<std::string> enumerateDirectories(const std::filesystem::path& directoryPath);
Result<std::string> fileToString(const std::filesystem::path& filePath);
Result<> stringToFile(const std::filesystem::path& outputFilePath, const std::string& str);

Result<std::vector<char>> readBinaryFile(const std::filesystem::path& filePath);
Result<> writeBinaryFile(const std::filesystem::path& filePath, std::span<const char> data);
} // namespace crisp
