#pragma once

#include <Crisp/Vulkan/VulkanBufferView.hpp>

#include <vulkan/vulkan.h>

namespace crisp
{
class VulkanDevice;
class VulkanCommandPool;

class VulkanCommandBuffer
{
public:
    explicit VulkanCommandBuffer(VkCommandBuffer commandBuffer);
    ~VulkanCommandBuffer();

    void begin(VkCommandBufferUsageFlags commandBufferUsage) const;
    void begin(VkCommandBufferUsageFlags commandBufferUsage, const VkCommandBufferInheritanceInfo* inheritance) const;

    void end() const;

    inline VkCommandBuffer getHandle() const
    {
        return m_handle;
    }

    void transferOwnership(VkBuffer buffer, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const;
    void insertPipelineBarrier(const VulkanBufferView& bufferView, VkPipelineStageFlags srcStage,
        VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess) const;
    void copyBuffer(const VulkanBufferView& srcBufferView, VkBuffer dstBuffer) const;

private:
    VkCommandBuffer m_handle;
};
} // namespace crisp