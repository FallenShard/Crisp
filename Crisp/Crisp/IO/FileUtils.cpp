#include <Crisp/Io/FileUtils.hpp>

#include <fstream>
#include <sstream>

namespace crisp {

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

    const auto fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

Result<> writeBinaryFile(const std::filesystem::path& filePath, const std::span<const char> data) {
    std::ofstream file(filePath, std::ios::binary);

    if (!file.is_open()) {
        return resultError("Failed to open binary file: {}!", filePath.string());
    }

    file.write(data.data(), static_cast<std::streamsize>(data.size()));

    return kResultSuccess;
}
} // namespace crisp