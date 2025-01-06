#pragma once

#include <thread>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>

namespace crisp {
class VulkanWorker {
public:
    VulkanWorker(VulkanDevice& device, const VulkanQueue& queue, uint32_t virtualFrameCount);
    ~VulkanWorker() = default;

    VulkanWorker(const VulkanWorker&) = delete;
    VulkanWorker& operator=(const VulkanWorker&) = delete;

    VulkanWorker(VulkanWorker&&) noexcept = delete;
    VulkanWorker& operator=(VulkanWorker&&) noexcept = delete;

    VulkanCommandBuffer* getCmdBuffer(uint32_t virtualFrameIndex) const {
        return m_cmdBuffers[virtualFrameIndex].get();
    }

    VulkanCommandBuffer* resetAndGetCmdBuffer(const VulkanDevice& device, uint32_t virtualFrameIndex) const {
        m_cmdPools[virtualFrameIndex]->reset(device);
        return m_cmdBuffers[virtualFrameIndex].get();
    }

private:
    std::thread m_thread;

    std::vector<std::unique_ptr<VulkanCommandPool>> m_cmdPools;
    std::vector<std::unique_ptr<VulkanCommandBuffer>> m_cmdBuffers;
};
} // namespace crisp
