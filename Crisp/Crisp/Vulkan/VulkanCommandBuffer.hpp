#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <span>
#include <vector>

namespace crisp {
struct MemoryRegion {
    void* ptr;
    size_t size;
};

class VulkanCommandBuffer {
public:
    enum class State {
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

    inline VkCommandBuffer getHandle() const {
        return m_handle;
    }

    inline State getState() const {
        return m_state;
    }

    void transferOwnership(VkBuffer buffer, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const;
    void insertMemoryBarrier(
        VkPipelineStageFlags srcStage,
        VkAccessFlags srcAccess,
        VkPipelineStageFlags dstStage,
        VkAccessFlags dstAccess) const;
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

    void dispatchCompute(const glm::ivec3& workGroupCount) const;

private:
    VkCommandBuffer m_handle;
    State m_state;
};

} // namespace crisp