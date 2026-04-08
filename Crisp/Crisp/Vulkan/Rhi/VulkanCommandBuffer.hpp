#pragma once

#include <span>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

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

    void insertBarrier(const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(
        VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(const VkDescriptorBufferInfo& bufferInfo, const VulkanSynchronizationScope& scope)
        const;
    void insertBufferMemoryBarriers(
        std::span<const VkBufferMemoryBarrier2> barriers) const;
    void insertImageMemoryBarrier(const VkImageMemoryBarrier2& barrier) const;

    void transferOwnership(
        VkBuffer buffer, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex,
        const VulkanSynchronizationScope& scope) const;

    void executeSecondaryBuffers(const std::vector<VkCommandBuffer>& commandBuffers) const;

    void updateBuffer(const VkDescriptorBufferInfo& bufferInfo, const MemoryRegion& memoryRegion) const;
    void copyBuffer(const VkDescriptorBufferInfo& srcBufferInfo, VkBuffer dstBuffer) const;

    void dispatchCompute(const VkExtent3D& workGroupCount) const;

private:
    VkCommandBuffer m_handle;
    State m_state;
};

} // namespace crisp
