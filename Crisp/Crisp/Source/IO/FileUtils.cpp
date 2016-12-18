#include "FileUtils.hpp"

#define NOMINMAX
#include <Windows.h>

#include <fstream>
#include <iostream>

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

    std::string FileUtils::fileToString(const std::string& filePath)
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
            std::cerr << "Could not open file: " + filePath << std::endl;
        }

        return source;
    }

    std::vector<char> FileUtils::readBinaryFile(const std::string& filePath)
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

    std::string FileUtils::openFileDialog()
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
        return std::string(fileName);
    }
}