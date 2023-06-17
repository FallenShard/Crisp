#include <Crisp/IO/FileUtils.hpp>

// #include <commdlg.h>

#include <fstream>
#include <sstream>

namespace crisp
{
std::vector<std::string> enumerateFiles(const std::filesystem::path& directory, const std::string& extension)
{
    std::vector<std::string> filenames;

    std::string pattern = '.' + extension;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.path().extension() == pattern)
        {
            filenames.push_back(entry.path().filename().string());
        }
    }

    return filenames;
}

std::vector<std::string> enumerateDirectories(const std::filesystem::path& directory)
{
    std::vector<std::string> dirNames;

    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_directory())
        {
            dirNames.push_back(entry.path().stem().generic_string());
        }
    }

    return dirNames;
}

Result<std::string> fileToString(const std::filesystem::path& filePath)
{
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open())
    {
        return resultError("Failed to open text file: {}!", filePath.string());
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    return buffer.str();
}

Result<> stringToFile(const std::filesystem::path& outputFilePath, const std::string& str)
{
    std::ofstream outputFile(outputFilePath);
    if (!outputFile.is_open())
    {
        return resultError("Failed to open text file: {}!", outputFilePath.string());
    }

    outputFile << str;
    return kResultSuccess;
}

Result<std::vector<char>> readBinaryFile(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return resultError("Failed to open binary file: {}!", filePath.string());
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

std::string openFileDialog()
{
    /*const int bufferSize = MAX_PATH;
    char oldDir[bufferSize];
    GetCurrentDirectory(bufferSize, oldDir);

    char fileNameBuffer[1024] = {0};

    OPENFILENAME openFile{};
    openFile.lStructSize = sizeof(OPENFILENAME);
    openFile.lpstrFile = fileNameBuffer;
    openFile.nMaxFile = 1024;
    openFile.Flags = OFN_FILEMUSTEXIST;
    GetOpenFileName(&openFile);

    char fileName[128] = {0};
    for (int i = openFile.nFileOffset; fileNameBuffer[i] != '\0'; i++)
    {
        fileName[i - openFile.nFileOffset] = fileNameBuffer[i];
    }

    SetCurrentDirectory(oldDir);
    return std::string(fileNameBuffer);*/
    return "";
}
} // namespace crisp