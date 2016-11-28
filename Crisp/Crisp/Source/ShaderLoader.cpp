#include "ShaderLoader.hpp"

#include "Core/FileUtils.hpp"

namespace crisp
{
    namespace
    {
        const std::string shaderPath = "Resources/Shaders/Vulkan/";
    }

    VkShaderModule ShaderLoader::load(VkDevice device, const std::string& vulkanShaderFile)
    {
        auto shaderCode = FileUtils::readBinaryFile(shaderPath + vulkanShaderFile);

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VkShaderModule shaderModule(VK_NULL_HANDLE);
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
        return shaderModule;
    }
}