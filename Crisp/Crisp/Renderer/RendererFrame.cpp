#include <Crisp/Renderer/RendererFrame.hpp>

namespace crisp {
RendererFrame::RendererFrame(const VulkanDevice& device, const int32_t logicalIndex)
    : m_completionFence(device.createFence(0))
    , m_imageAvailableSemaphore(device.createSemaphore())
    , m_renderFinishedSemaphore(device.createSemaphore())
    , m_deviceHandle(device.getHandle())
    , m_logicalIndex(logicalIndex) {
    device.setObjectName(m_completionFence, fmt::format("[Frame {}] Frame Completion Fence", m_logicalIndex));
    device.setObjectName(m_imageAvailableSemaphore, fmt::format("[Frame {}] Image Available Sem", m_logicalIndex));
    device.setObjectName(m_renderFinishedSemaphore, fmt::format("[Frame {}] Render Finished Sem", m_logicalIndex));
}

RendererFrame::~RendererFrame() {
    if (m_completionFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_deviceHandle, m_completionFence, nullptr);
    }
    if (m_imageAvailableSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_deviceHandle, m_imageAvailableSemaphore, nullptr);
    }
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_deviceHandle, m_renderFinishedSemaphore, nullptr);
    }
}

RendererFrame::RendererFrame(RendererFrame&& other) noexcept
    : m_completionFence(std::exchange(other.m_completionFence, VK_NULL_HANDLE))
    , m_imageAvailableSemaphore(std::exchange(other.m_imageAvailableSemaphore, VK_NULL_HANDLE))
    , m_renderFinishedSemaphore(std::exchange(other.m_renderFinishedSemaphore, VK_NULL_HANDLE))
    , m_status(other.m_status)
    , m_deviceHandle(other.m_deviceHandle)
    , m_logicalIndex(other.m_logicalIndex)
    , m_submissions(std::move(other.m_submissions)) {}

RendererFrame& RendererFrame::operator=(RendererFrame&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    m_completionFence = std::exchange(other.m_completionFence, VK_NULL_HANDLE);
    m_imageAvailableSemaphore = std::exchange(other.m_imageAvailableSemaphore, VK_NULL_HANDLE);
    m_renderFinishedSemaphore = std::exchange(other.m_renderFinishedSemaphore, VK_NULL_HANDLE);
    m_status = other.m_status;
    m_deviceHandle = other.m_deviceHandle;
    m_logicalIndex = other.m_logicalIndex;
    m_submissions = std::move(other.m_submissions);
    return *this;
}

void RendererFrame::waitCompletion(const VulkanDevice& device) {
    if (m_status == Status::Idle) {
        return;
    }

    vkWaitForFences(device.getHandle(), 1, &m_completionFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkResetFences(device.getHandle(), 1, &m_completionFence);
    m_status = Status::Idle;
}

void RendererFrame::addSubmission(const VulkanCommandBuffer& cmdBuffer) {
    Submission submission{};
    submission.cmdBufferHandles.push_back(cmdBuffer.getHandle());
    submission.waitSemaphores.push_back(m_imageAvailableSemaphore);
    submission.waitStages.push_back(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    submission.signalSemaphores.push_back(m_renderFinishedSemaphore);
    // Waited on by vkQueuePresentKHR, so this stays broad on purpose. Narrowing to COLOR_ATTACHMENT_OUTPUT would
    // be correct only while a render pass is the last thing to write the swapchain image - a compute or blit pass
    // writing it afterwards is not logically earlier, and would race. There is also nothing to gain: the waiter is
    // the display engine at end of frame, with no subsequent GPU work to overlap.
    submission.signalStages.push_back(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    m_submissions.push_back(submission);
}

void RendererFrame::submitToQueue(const VulkanQueue& queue) {
    // The per-submission info arrays are held in these outer vectors, sized up front, because VkSubmitInfo2
    // stores pointers into them and must stay valid until vkQueueSubmit2 returns.
    std::vector<std::vector<VkSemaphoreSubmitInfo>> waitInfos(m_submissions.size());
    std::vector<std::vector<VkSemaphoreSubmitInfo>> signalInfos(m_submissions.size());
    std::vector<std::vector<VkCommandBufferSubmitInfo>> cmdBufferInfos(m_submissions.size());
    std::vector<VkSubmitInfo2> submitInfos{};
    submitInfos.reserve(m_submissions.size());

    for (size_t i = 0; i < m_submissions.size(); ++i) {
        const auto& submission = m_submissions[i];
        for (size_t j = 0; j < submission.waitSemaphores.size(); ++j) {
            waitInfos[i].push_back({
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = submission.waitSemaphores[j],
                .stageMask = submission.waitStages[j],
            });
        }
        for (size_t j = 0; j < submission.signalSemaphores.size(); ++j) {
            signalInfos[i].push_back({
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = submission.signalSemaphores[j],
                .stageMask = submission.signalStages[j],
            });
        }
        for (const auto cmdBuffer : submission.cmdBufferHandles) {
            cmdBufferInfos[i].push_back({
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmdBuffer,
            });
        }

        submitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = static_cast<uint32_t>(waitInfos[i].size()),
            .pWaitSemaphoreInfos = waitInfos[i].data(),
            .commandBufferInfoCount = static_cast<uint32_t>(cmdBufferInfos[i].size()),
            .pCommandBufferInfos = cmdBufferInfos[i].data(),
            .signalSemaphoreInfoCount = static_cast<uint32_t>(signalInfos[i].size()),
            .pSignalSemaphoreInfos = signalInfos[i].data(),
        });
    }

    vkQueueSubmit2(queue.getHandle(), static_cast<uint32_t>(submitInfos.size()), submitInfos.data(), m_completionFence);
    m_submissions.clear();
    m_status = Status::Submitted;
}

VkSemaphore RendererFrame::getImageAvailableSemaphoreHandle() const {
    return m_imageAvailableSemaphore;
}

VkSemaphore RendererFrame::getRenderFinishedSemaphoreHandle() const {
    return m_renderFinishedSemaphore;
}

} // namespace crisp
