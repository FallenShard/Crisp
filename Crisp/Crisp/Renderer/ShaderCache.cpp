#include <Crisp/Renderer/ShaderCache.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/IO/FileUtils.hpp>

namespace crisp
{

auto logger = createLoggerSt("ShaderCache");

ShaderCache::ShaderCache(VkDevice deviceHandle)
    : m_deviceHandle(deviceHandle)
{
}

ShaderCache::~ShaderCache()
{
    for (auto& shaderModule : m_shaderModules)
    {
        vkDestroyShaderModule(m_deviceHandle, shaderModule.second.handle, nullptr);
    }
}

VkShaderModule ShaderCache::loadSpirvShaderModule(const std::filesystem::path& shaderModulePath)
{
    const auto timestamp = std::filesystem::last_write_time(shaderModulePath);
    const auto shaderKey = shaderModulePath.stem().string();
    const auto existingItem = m_shaderModules.find(shaderKey);
    if (existingItem != m_shaderModules.end())
    {
        // Cached shader module is same or newer
        if (existingItem->second.lastModifiedTimestamp >= timestamp)
        {
            return existingItem->second.handle;
        }
        else
        {
            logger->info("Reloading shader module: {}", shaderKey);
            vkDestroyShaderModule(m_deviceHandle, existingItem->second.handle, nullptr);
        }
    }

    const auto shaderCode = readBinaryFile(shaderModulePath).unwrap();
    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule(VK_NULL_HANDLE);
    vkCreateShaderModule(m_deviceHandle, &createInfo, nullptr, &shaderModule);

    m_shaderModules.insert_or_assign(shaderKey, ShaderModule{shaderModule, timestamp});
    return shaderModule;
}
} // namespace crisp
