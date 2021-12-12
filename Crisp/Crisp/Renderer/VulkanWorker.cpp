#include <Crisp/Renderer/VulkanWorker.hpp>

#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp
{
    VulkanWorker::VulkanWorker(VulkanDevice& device, const VulkanQueue& queue, uint32_t virtualFrameCount)
        : m_cmdPools(virtualFrameCount)
        , m_cmdBuffers(virtualFrameCount)
    {
        for (uint32_t i = 0; i < virtualFrameCount; ++i)
        {
            m_cmdPools[i] = std::make_unique<VulkanCommandPool>(queue.createCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT), device.getResourceDeallocator());
            m_cmdBuffers[i] = std::make_unique<VulkanCommandBuffer>(m_cmdPools[i]->allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        }
    }

    VulkanWorker::~VulkanWorker()
    {
    }
}