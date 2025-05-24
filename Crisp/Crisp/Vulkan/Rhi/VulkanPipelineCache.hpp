#pragma once

#include <filesystem>

#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {

class VulkanDevice;

class VulkanPipelineCache final : public VulkanResource<VkPipelineCache> {
public:
    VulkanPipelineCache(const VulkanDevice& device, std::filesystem::path&& cachePath);
    ~VulkanPipelineCache();

    VulkanPipelineCache(const VulkanPipelineCache&) = delete;
    VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;

    VulkanPipelineCache(VulkanPipelineCache&&) = default;
    VulkanPipelineCache& operator=(VulkanPipelineCache&&) = default;

private:
    std::filesystem::path m_cachePath;
};
} // namespace crisp