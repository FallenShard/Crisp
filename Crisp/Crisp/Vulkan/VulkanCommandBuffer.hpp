#pragma once

#include <Crisp/Vulkan/VulkanBufferView.hpp>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/MemoryRegion.hpp>

#include <vulkan/vulkan.h>

#include <span>
#include <vector>

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
    void insertBufferMemoryBarrier(
        const VulkanBufferSpan& bufferSpan,
        VkPipelineStageFlags srcStage,
        VkAccessFlags srcAccess,
        VkPipelineStageFlags dstStage,
        VkAccessFlags dstAccess) const;
    void insertBufferMemoryBarrier(
        const VkBufferMemoryBarrier& barrier, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const;
    void insertBufferMemoryBarriers(
        std::span<VkBufferMemoryBarrier> barriers, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const;
    void insertImageMemoryBarrier(
        const VkImageMemoryBarrier& barrier, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const;
    // void insertImageMemoryBarrier(const VulkanBufferView& bufferView, VkPipelineStageFlags srcStage,
    //     VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess) const;
    // void insertMemoryBarrier(VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage,
    //     VkAccessFlags dstAccess) const;

    void executeSecondaryBuffers(const std::vector<VkCommandBuffer>& commandBuffers) const;

    void updateBuffer(const VulkanBufferSpan& bufferSpan, const MemoryRegion& memoryRegion) const;
    void copyBuffer(const VulkanBufferSpan& srcBufferSpan, VkBuffer dstBuffer) const;

    void dispatchCompute(const glm::ivec3& workGroupCount) const;

private:
    VkCommandBuffer m_handle;
};
} // namespace crisp