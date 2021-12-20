#pragma once

#include "vulkan/vulkan.h"
#include <memory>

#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>

namespace crisp
{
struct VirtualFrame
{
    VkFence completionFence;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    enum class Status
    {
        Free,
        Submitted
    };

    Status status = Status::Free;

    std::vector<VkSubmitInfo> submitInfos;
    std::list<std::vector<VkPipelineStageFlags>> waitStages;
    std::list<std::vector<VkSemaphore>> waitSemaphores;
    std::list<std::vector<VkSemaphore>> signalSemaphores;
    std::list<std::vector<VkCommandBuffer>> cmdBuffers;

    inline void waitCompletion(VkDevice device)
    {
        if (status == Status::Submitted)
        {
            vkWaitForFences(device, 1, &completionFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
            vkResetFences(device, 1, &completionFence);
            status = Status::Free;
        }
    }

    inline void addSubmission(const VulkanCommandBuffer& cmdBuffer)
    {
        waitStages.push_back({ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT });
        waitSemaphores.push_back({ imageAvailableSemaphore });
        signalSemaphores.push_back({ renderFinishedSemaphore });
        cmdBuffers.push_back({ cmdBuffer.getHandle() });

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.back().size());
        submitInfo.pWaitSemaphores = waitSemaphores.back().data();
        submitInfo.pWaitDstStageMask = waitStages.back().data();
        submitInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.back().size());
        submitInfo.pCommandBuffers = cmdBuffers.back().data();
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.back().size());
        submitInfo.pSignalSemaphores = signalSemaphores.back().data();
        submitInfos.push_back(submitInfo);
    }

    inline void submitToQueue(const VulkanQueue& queue)
    {
        vkQueueSubmit(queue.getHandle(), static_cast<uint32_t>(submitInfos.size()), submitInfos.data(),
            completionFence);
        submitInfos.clear();
        waitStages.clear();
        waitSemaphores.clear();
        signalSemaphores.clear();
        cmdBuffers.clear();
        status = Status::Submitted;
    }
};
} // namespace crisp
