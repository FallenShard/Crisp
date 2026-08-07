#include <Crisp/Vulkan/Rhi/VulkanCommandBuffer.hpp>

namespace crisp {
VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer commandBuffer)
    : m_handle(commandBuffer)
    , m_state(State::Idle) {}

void VulkanCommandBuffer::setIdleState() {
    CRISP_CHECK(m_state == State::Executing || m_state == State::Idle);
    m_state = State::Idle;
}

void VulkanCommandBuffer::begin(const VkCommandBufferUsageFlags commandBufferUsage) {
    CRISP_CHECK_EQ(m_state, State::Idle);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = commandBufferUsage;
    beginInfo.pInheritanceInfo = nullptr;

    vkBeginCommandBuffer(m_handle, &beginInfo);
    m_state = State::Recording;
}

void VulkanCommandBuffer::begin(
    const VkCommandBufferUsageFlags commandBufferUsage, const VkCommandBufferInheritanceInfo* inheritance) {
    CRISP_CHECK_EQ(m_state, State::Idle);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = commandBufferUsage;
    beginInfo.pInheritanceInfo = inheritance;

    vkBeginCommandBuffer(m_handle, &beginInfo);
    m_state = State::Recording;
}

void VulkanCommandBuffer::end() {
    CRISP_CHECK_EQ(m_state, State::Recording);
    vkEndCommandBuffer(m_handle);
    m_state = State::Pending;
}

void VulkanCommandBuffer::setExecutionState() {
    CRISP_CHECK_EQ(m_state, State::Pending);
    m_state = State::Executing;
}
} // namespace crisp
