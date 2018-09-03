#include "FileUtils.hpp"

#include <Windows.h>

#include <fstream>

#include <CrispCore/Log.hpp>

namespace crisp::fileutils
{
    std::vector<std::string> enumerateFiles(const std::filesystem::path& directory, const std::string& extension)
    {
        std::vector<std::string> filenames;

        std::string pattern = '.' + extension;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.path().extension() == pattern)
                filenames.push_back(entry.path().filename().string());
        }

        return filenames;
    }

    std::string fileToString(const std::string& filePath)
    {
        std::ifstream inputFile(filePath);
        std::string source;

        if (inputFile.is_open())
        {
            std::string line;
            while (std::getline(inputFile, line))
            {
                source += line + '\n';
            }
            inputFile.close();
        }
        else
        {
            logError("Could not open file: ", filePath);
        }

        return source;
    }

    std::vector<char> readBinaryFile(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        if (!file.is_open())
            throw std::exception("Failed to open file!");

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    std::string openFileDialog()
    {
        const int bufferSize = MAX_PATH;
        char oldDir[bufferSize];
        GetCurrentDirectory(bufferSize, oldDir);

        char fileNameBuffer[1024] = { 0 };

        OPENFILENAME openFile;
        openFile.lStructSize       = sizeof(OPENFILENAME);
        openFile.hwndOwner         = nullptr;
        openFile.hInstance         = nullptr;
        openFile.lpstrFilter       = nullptr;
        openFile.lpstrCustomFilter = nullptr;
        openFile.nMaxCustFilter    = 0;
        openFile.nFilterIndex      = 0;
        openFile.lpstrFile         = fileNameBuffer;
        openFile.nMaxFile          = 1024;
        openFile.lpstrFileTitle    = nullptr;
        openFile.nMaxFileTitle     = 0;
        openFile.lpstrInitialDir   = nullptr;
        openFile.lpstrTitle        = nullptr;
        openFile.Flags             = OFN_FILEMUSTEXIST;
        openFile.nFileOffset       = 0;
        openFile.nFileExtension    = 0;
        openFile.lpstrDefExt       = nullptr;

        GetOpenFileName(&openFile);

        char fileName[128] = { 0 };
        for (int i = openFile.nFileOffset; fileNameBuffer[i] != '\0'; i++)
            fileName[i - openFile.nFileOffset] = fileNameBuffer[i];

        SetCurrentDirectory(oldDir);
        return std::string(fileNameBuffer);
    }

    std::string getFileNameFromPath(const std::string& filePath)
    {
        auto result = filePath;
        auto indexPos = result.find_last_of("\\/");
        if (indexPos != std::string::npos)
            result.erase(0, indexPos + 1);

        auto extensionDotPos = result.rfind('.');
        if (extensionDotPos != std::string::npos)
            result.erase(extensionDotPos);

        return result;
    }

    void createDirectory(const std::string& path)
    {
        CreateDirectory(path.c_str(), nullptr);
    }

    bool fileExists(const std::string& filePath)
    {
        return std::filesystem::exists(std::filesystem::path(filePath));
    }
}