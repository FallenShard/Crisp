#pragma once

#include <Crisp/Vulkan/Rhi/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
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

private:
    VkCommandBuffer m_handle;
    State m_state;
};

} // namespace crisp
