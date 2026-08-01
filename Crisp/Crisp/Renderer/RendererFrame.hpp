#pragma once

#include <Crisp/Vulkan/Rhi/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>
#include <Crisp/Vulkan/Rhi/VulkanTimelineSemaphore.hpp>

namespace crisp {
class RendererFrame {
public:
    explicit RendererFrame(const VulkanDevice& device, int32_t logicalIndex);
    ~RendererFrame();

    RendererFrame(const RendererFrame&) = delete;
    RendererFrame(RendererFrame&& other) noexcept;

    RendererFrame& operator=(const RendererFrame&) = delete;
    RendererFrame& operator=(RendererFrame&&) noexcept;

    void waitCompletion(const VulkanTimelineSemaphore& timeline) const;
    void addSubmission(const VulkanCommandBuffer& cmdBuffer);
    uint64_t submitToQueue(const VulkanQueue& queue, VulkanTimelineSemaphore& timeline);

    VkSemaphore getImageAvailableSemaphoreHandle() const;
    VkSemaphore getRenderFinishedSemaphoreHandle() const;

private:
    VkSemaphore m_imageAvailableSemaphore;
    VkSemaphore m_renderFinishedSemaphore;
    VkDevice m_deviceHandle; // Non-owning.
    int32_t m_logicalIndex;

    // Timeline value this slot's last submission signals. Zero until first use, which the timeline starts at, so
    // the first wait resolves immediately instead of needing an explicit "not yet submitted" state.
    uint64_t m_submittedValue{0};

    struct SemaphoreOperation {
        VkSemaphore semaphore{VK_NULL_HANDLE};
        VkPipelineStageFlags2 stage{VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
        uint64_t value{0}; // Ignored for binary semaphores.
    };

    struct Submission {
        std::vector<VkCommandBuffer> cmdBufferHandles;
        std::vector<SemaphoreOperation> waits;
        std::vector<SemaphoreOperation> signals;
    };

    std::vector<Submission> m_submissions;
};
} // namespace crisp
