#pragma once

#include <memory>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>

namespace crisp {
class RendererFrame {
public:
    enum class Status { Idle, Submitted };

    explicit RendererFrame(const VulkanDevice& device, int32_t logicalIndex);
    ~RendererFrame();

    RendererFrame(const RendererFrame&) = delete;
    RendererFrame(RendererFrame&& other) noexcept;

    RendererFrame& operator=(const RendererFrame&) = delete;
    RendererFrame& operator=(RendererFrame&&) noexcept;

    void waitCompletion(VkDevice device);
    void addSubmission(const VulkanCommandBuffer& cmdBuffer);
    void submitToQueue(const VulkanQueue& queue);

    VkSemaphore getImageAvailableSemaphoreHandle() const;
    VkSemaphore getRenderFinishedSemaphoreHandle() const;

private:
    VkFence m_completionFence;
    VkSemaphore m_imageAvailableSemaphore;
    VkSemaphore m_renderFinishedSemaphore;
    Status m_status{Status::Idle};
    VkDevice m_deviceHandle; // Non-owning.
    int32_t m_logicalIndex;

    struct Submission {
        std::vector<VkCommandBuffer> cmdBufferHandles;

        std::vector<VkSemaphore> waitSemaphores;
        std::vector<VkPipelineStageFlags> waitStages;

        std::vector<VkSemaphore> signalSemaphores;
    };

    std::vector<Submission> m_submissions;
};
} // namespace crisp
