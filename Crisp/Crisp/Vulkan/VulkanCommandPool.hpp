#pragma once

#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp
{
class VulkanDevice;
class VulkanQueue;

class VulkanCommandPool final : public VulkanResource<VkCommandPool>
{
public:
    VulkanCommandPool(VkCommandPool handle, VulkanResourceDeallocator& deallocator);

    VkCommandBuffer allocateCommandBuffer(const VulkanDevice& device, VkCommandBufferLevel cmdBufferLevel) const;
};
} // namespace crisp