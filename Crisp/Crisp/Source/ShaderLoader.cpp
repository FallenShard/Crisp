#include "ShaderLoader.hpp"

#include <iostream>
#include <fstream>
#include <vector>

namespace crisp
{
    namespace
    {
        const std::string shaderPath = "Resources/Shaders/";

        std::string fileToString(std::string filePath)
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

            return source;
        }

        std::vector<char> readFile(const std::string& filename)
        {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open())
                throw std::exception("Failed to open file!");

            size_t fileSize = static_cast<size_t>(file.tellg());
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);
            file.close();

            return buffer;
        }
    }

    VkShaderModule ShaderLoader::load(VkDevice device, const std::string& vulkanShaderFile)
    {
        auto shaderCode = readFile(vulkanShaderFile);

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VkShaderModule shaderModule(VK_NULL_HANDLE);
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
        return shaderModule;
    }
}