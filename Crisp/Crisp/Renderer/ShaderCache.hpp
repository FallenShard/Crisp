#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace crisp {

class ShaderCache {
public:
    explicit ShaderCache(VulkanDevice* device);
    ~ShaderCache();

    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;

    ShaderCache(ShaderCache&&) noexcept = delete;
    ShaderCache& operator=(ShaderCache&&) noexcept = delete;

    inline std::optional<VkShaderModule> getShaderModule(const std::string& key) const {
        const auto it = m_shaderModules.find(key);
        if (it == m_shaderModules.end()) {
            return std::nullopt;
        }
        return it->second.handle;
    }

    VkShaderModule getOrLoadShaderModule(const std::filesystem::path& spvShaderPath);

private:
    struct ShaderModule {
        VkShaderModule handle{VK_NULL_HANDLE};
        std::filesystem::file_time_type lastModifiedTimestamp;
    };

    VulkanDevice* m_device{nullptr};
    FlatHashMap<std::string, ShaderModule> m_shaderModules;
};

} // namespace crisp