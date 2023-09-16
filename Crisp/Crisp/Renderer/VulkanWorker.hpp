#pragma once

#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>

#include <thread>
#include <vector>

namespace crisp
{
class VulkanWorker
{
public:
    VulkanWorker(VulkanDevice& device, const VulkanQueue& queue, uint32_t virtualFrameCount);
    ~VulkanWorker() = default;

    VulkanWorker(const VulkanWorker&) = delete;
    VulkanWorker& operator=(const VulkanWorker&) = delete;

    VulkanWorker(VulkanWorker&&) noexcept = delete;
    VulkanWorker& operator=(VulkanWorker&&) noexcept = delete;

    inline VulkanCommandBuffer* getCmdBuffer(uint32_t virtualFrameIndex) const
    {
        return m_cmdBuffers[virtualFrameIndex].get();
    }

private:
    std::thread m_thread;

    std::vector<std::unique_ptr<VulkanCommandPool>> m_cmdPools;
    std::vector<std::unique_ptr<VulkanCommandBuffer>> m_cmdBuffers;
};
} // namespace crisp
