#include <Crisp/Renderer/ShaderCache.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Io/FileUtils.hpp>

namespace crisp {
CRISP_MAKE_LOGGER_ST("ShaderCache");

ShaderCache::ShaderCache(VulkanDevice* device)
    : m_device(device) {}

ShaderCache::~ShaderCache() {
    for (auto& shaderModule : m_shaderModules) {
        vkDestroyShaderModule(m_device->getHandle(), shaderModule.second.handle, nullptr);
    }
}

VkShaderModule ShaderCache::getOrLoadShaderModule(const std::filesystem::path& spvShaderPath) {
    const auto timestamp = std::filesystem::last_write_time(spvShaderPath);
    const auto shaderKey = spvShaderPath.stem().string();
    const auto existingItem = m_shaderModules.find(shaderKey);
    if (existingItem != m_shaderModules.end()) {
        // Cached shader module is same or newer
        if (existingItem->second.lastModifiedTimestamp >= timestamp) {
            return existingItem->second.handle;
        }

        CRISP_LOGI("Reloading shader module: {}", shaderKey);
        vkDestroyShaderModule(m_device->getHandle(), existingItem->second.handle, nullptr);
    }

    const auto shaderCode = readBinaryFile(spvShaderPath).unwrap();
    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data()); // NOLINT

    VkShaderModule shaderModule(VK_NULL_HANDLE);
    vkCreateShaderModule(m_device->getHandle(), &createInfo, nullptr, &shaderModule);

    m_shaderModules.insert_or_assign(shaderKey, ShaderModule{shaderModule, timestamp});
    m_device->getDebugMarker().setObjectName(shaderModule, shaderKey);
    return shaderModule;
}
} // namespace crisp
