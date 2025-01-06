#include <Crisp/Renderer/VulkanWorker.hpp>

namespace crisp {
VulkanWorker::VulkanWorker(VulkanDevice& device, const VulkanQueue& queue, uint32_t virtualFrameCount)
    : m_cmdPools(virtualFrameCount)
    , m_cmdBuffers(virtualFrameCount) {
    for (uint32_t i = 0; i < virtualFrameCount; ++i) {
        m_cmdPools[i] = std::make_unique<VulkanCommandPool>(queue.createCommandPool(0), device.getResourceDeallocator());
        m_cmdBuffers[i] = std::make_unique<VulkanCommandBuffer>(
            m_cmdPools[i]->allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        device.getDebugMarker().setObjectName(m_cmdPools[i]->getHandle(), fmt::format("[Frame {}] Command Pool", i));
        device.getDebugMarker().setObjectName(
            m_cmdBuffers[i]->getHandle(), fmt::format("[Frame {}] Primary Command Buffer", i));
    }
}
} // namespace crisp