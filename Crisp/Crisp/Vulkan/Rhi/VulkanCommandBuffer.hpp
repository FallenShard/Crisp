#pragma once

#include <span>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
struct MemoryRegion {
    void* ptr;
    size_t size;
};

class VulkanCommandBuffer {
public:
    enum class State : uint8_t {
        Idle,
        Recording,
        Pending,
        Executing,
    };

    explicit VulkanCommandBuffer(VkCommandBuffer commandBuffer);

    void setIdleState();

    void begin(VkCommandBufferUsageFlags commandBufferUsage);
    void begin(VkCommandBufferUsageFlags commandBufferUsage, const VkCommandBufferInheritanceInfo* inheritance);

    void end();

    void setExecutionState();

    VkCommandBuffer getHandle() const {
        return m_handle;
    }

    State getState() const {
        return m_state;
    }

    void transferOwnership(VkBuffer buffer, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const;
    void insertMemoryBarrier(
        VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
        const;
    void insertBufferMemoryBarrier(
        const VkDescriptorBufferInfo& bufferInfo,
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

    void updateBuffer(const VkDescriptorBufferInfo& bufferInfo, const MemoryRegion& memoryRegion) const;
    void copyBuffer(const VkDescriptorBufferInfo& srcBufferInfo, VkBuffer dstBuffer) const;

    void dispatchCompute(const VkExtent3D& workGroupCount) const;

private:
    VkCommandBuffer m_handle;
    State m_state;
};

} // namespace crisp