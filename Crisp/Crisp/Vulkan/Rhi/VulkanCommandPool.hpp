#pragma once

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {
class VulkanCommandPool final : public VulkanResource<VkCommandPool> {
public:
    VulkanCommandPool(VkCommandPool handle, VulkanResourceDeallocator& deallocator);

    VkCommandBuffer allocateCommandBuffer(const VulkanDevice& device, VkCommandBufferLevel cmdBufferLevel) const;

    void reset(const VulkanDevice& device);
};
} // namespace crisp