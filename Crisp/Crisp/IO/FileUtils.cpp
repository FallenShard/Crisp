#include <Crisp/Io/FileUtils.hpp>

#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>

namespace crisp {

Result<std::optional<std::filesystem::file_time_type>> getLastWriteTime(const std::filesystem::path& path) {
    std::error_code error;
    const auto timestamp = std::filesystem::last_write_time(path, error);
    if (!error) {
        return std::optional{timestamp};
    }
    if (error == std::errc::no_such_file_or_directory) {
        return std::optional<std::filesystem::file_time_type>{};
    }
    return resultError("Failed to query the last write time of {}: {}", path.string(), error.message());
}

std::vector<std::string> enumerateDirectories(const std::filesystem::path& directory) {
    std::vector<std::string> dirNames;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_directory()) {
            dirNames.push_back(entry.path().stem().generic_string());
        }
    }

    return dirNames;
}

Result<std::string> fileToString(const std::filesystem::path& filePath) {
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        return resultError("Failed to open text file: {}!", filePath.string());
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    return buffer.str();
}

Result<> stringToFile(const std::filesystem::path& outputFilePath, const std::string& str) {
    std::ofstream outputFile(outputFilePath);
    if (!outputFile.is_open()) {
        return resultError("Failed to open text file: {}!", outputFilePath.string());
    }

    outputFile << str;
    return kResultSuccess;
}

Result<std::vector<char>> readBinaryFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return resultError("Failed to open binary file: {}!", filePath.string());
    }

    const auto fileSizeOffset = static_cast<std::streamoff>(file.tellg());
    if (fileSizeOffset < 0) {
        return resultError("Failed to determine the size of binary file: {}!", filePath.string());
    }

    const auto fileSize = static_cast<std::uintmax_t>(fileSizeOffset);
    if (fileSize > std::numeric_limits<std::size_t>::max() ||
        fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return resultError("Binary file is too large to read: {}!", filePath.string());
    }

    std::vector<char> buffer(static_cast<std::size_t>(fileSize));

    file.seekg(0, std::ios::beg);
    if (!file) {
        return resultError("Failed to seek to the beginning of binary file: {}!", filePath.string());
    }
    if (!buffer.empty() && !file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()))) {
        return resultError("Failed to read binary file: {}!", filePath.string());
    }

    return buffer;
}

Result<> writeBinaryFile(const std::filesystem::path& filePath, const std::span<const std::byte> data) {
    if (data.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return resultError("Binary data is too large to write to {}!", filePath.string());
    }

    std::ofstream file(filePath, std::ios::binary);

    if (!file.is_open()) {
        return resultError("Failed to open binary file: {}!", filePath.string());
    }

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size())); // NOLINT
    if (!file) {
        return resultError("Failed to write binary file: {}!", filePath.string());
    }

    return kResultSuccess;
}
} // namespace crisp
