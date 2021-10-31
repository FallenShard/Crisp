#include <Crisp/ShadingLanguage/ShaderPreprocessor.hpp>

#include <Crisp/IO/FileUtils.hpp>

#include <fstream>

namespace
{
    static std::vector<std::string> tokenize(const std::string& string, const std::string& delimiter)
    {
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = 0;

        while (end != std::string::npos)
        {
            end = string.find(delimiter, start);

            result.push_back(string.substr(start, (end == std::string::npos) ? std::string::npos : end - start));

            start = ((end > (std::string::npos - delimiter.size())) ? std::string::npos : end + delimiter.size());
        }

        return result;
    }
}

std::string crisp::preprocess(const std::filesystem::path& resourceFolder, const std::string& shaderFilename)
{
    std::ifstream file(resourceFolder / shaderFilename);

    auto readLines = [](const std::filesystem::path& path)
    {
        std::ifstream file(path);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line))
            lines.push_back(line);
        return lines;
    };

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line))
    {




        if (line.find("#include") == 0)
        {
            auto tokens = tokenize(line, " ");

            std::string filename = tokens[1].substr(1, tokens[1].size() - 2) + ".txt";

            auto parts = readLines(resourceFolder / "TestData" / filename);
            lines.insert(lines.end(), parts.begin(), parts.end());
        }
        else
            lines.push_back(line);
    }






    return std::string();
}
