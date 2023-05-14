#pragma once

#include <Crisp/Core/HashMap.hpp>

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <filesystem>
#include <string>

namespace crisp
{

class ShaderCache
{
public:
    ShaderCache(VkDevice deviceHandle);
    ~ShaderCache();

    inline VkShaderModule getShaderModuleHandle(const std::string& key) const
    {
        return m_shaderModules.at(key).handle;
    }

    VkShaderModule loadSpirvShaderModule(const std::filesystem::path& shaderModulePath);

private:
    struct ShaderModule
    {
        VkShaderModule handle;
        std::filesystem::file_time_type lastModifiedTimestamp;
    };

    VkDevice m_deviceHandle;
    FlatHashMap<std::string, ShaderModule> m_shaderModules;
};

} // namespace crisp