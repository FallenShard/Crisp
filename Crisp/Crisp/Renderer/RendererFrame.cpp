#include <Crisp/Renderer/RendererFrame.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
RendererFrame::RendererFrame(const VulkanDevice& device, const int32_t logicalIndex)
    : m_imageAvailableSemaphore(device.createSemaphore())
    , m_renderFinishedSemaphore(device.createSemaphore())
    , m_deviceHandle(device.getHandle())
    , m_logicalIndex(logicalIndex) {
    device.setObjectName(m_imageAvailableSemaphore, fmt::format("[Frame {}] Image Available Sem", m_logicalIndex));
    device.setObjectName(m_renderFinishedSemaphore, fmt::format("[Frame {}] Render Finished Sem", m_logicalIndex));
}

RendererFrame::~RendererFrame() {
    if (m_imageAvailableSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_deviceHandle, m_imageAvailableSemaphore, nullptr);
    }
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_deviceHandle, m_renderFinishedSemaphore, nullptr);
    }
}

RendererFrame::RendererFrame(RendererFrame&& other) noexcept
    : m_imageAvailableSemaphore(std::exchange(other.m_imageAvailableSemaphore, VK_NULL_HANDLE))
    , m_renderFinishedSemaphore(std::exchange(other.m_renderFinishedSemaphore, VK_NULL_HANDLE))
    , m_deviceHandle(other.m_deviceHandle)
    , m_logicalIndex(other.m_logicalIndex)
    , m_submittedValue(other.m_submittedValue)
    , m_submissions(std::move(other.m_submissions)) {}

RendererFrame& RendererFrame::operator=(RendererFrame&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    m_imageAvailableSemaphore = std::exchange(other.m_imageAvailableSemaphore, VK_NULL_HANDLE);
    m_renderFinishedSemaphore = std::exchange(other.m_renderFinishedSemaphore, VK_NULL_HANDLE);
    m_deviceHandle = other.m_deviceHandle;
    m_logicalIndex = other.m_logicalIndex;
    m_submittedValue = other.m_submittedValue;
    m_submissions = std::move(other.m_submissions);
    return *this;
}

void RendererFrame::waitCompletion(const VulkanTimelineSemaphore& timeline) const {
    timeline.wait(m_submittedValue);
}

void RendererFrame::addSubmission(const VulkanCommandBuffer& cmdBuffer) {
    Submission submission{};
    submission.cmdBufferHandles.push_back(cmdBuffer.getHandle());
    submission.waits.push_back({m_imageAvailableSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT});
    // Waited on by vkQueuePresentKHR, so this stays broad on purpose. Narrowing to COLOR_ATTACHMENT_OUTPUT would
    // be correct only while a render pass is the last thing to write the swapchain image - a compute or blit pass
    // writing it afterwards is not logically earlier and would need to be accounted for separately.
    submission.signals.push_back({m_renderFinishedSemaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT});
    m_submissions.push_back(submission);
}

uint64_t RendererFrame::submitToQueue(const VulkanQueue& queue, VulkanTimelineSemaphore& timeline) {
    CRISP_CHECK(!m_submissions.empty());

    m_submittedValue = timeline.advance();
    m_submissions.back().signals.push_back(
        {timeline.getHandle(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, m_submittedValue});

    // The per-submission info arrays are held in these outer vectors, sized up front, because VkSubmitInfo2
    // stores pointers into them and must stay valid until vkQueueSubmit2 returns.
    std::vector<std::vector<VkSemaphoreSubmitInfo>> waitInfos(m_submissions.size());
    std::vector<std::vector<VkSemaphoreSubmitInfo>> signalInfos(m_submissions.size());
    std::vector<std::vector<VkCommandBufferSubmitInfo>> cmdBufferInfos(m_submissions.size());
    std::vector<VkSubmitInfo2> submitInfos{};
    submitInfos.reserve(m_submissions.size());

    const auto toSemaphoreInfo = [](const SemaphoreOperation& op) {
        return VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = op.semaphore,
            .value = op.value,
            .stageMask = op.stage,
        };
    };

    for (size_t i = 0; i < m_submissions.size(); ++i) {
        const auto& submission = m_submissions[i];
        for (const auto& op : submission.waits) {
            waitInfos[i].push_back(toSemaphoreInfo(op));
        }
        for (const auto& op : submission.signals) {
            signalInfos[i].push_back(toSemaphoreInfo(op));
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

    VK_FATAL(vkQueueSubmit2(
        queue.getHandle(), static_cast<uint32_t>(submitInfos.size()), submitInfos.data(), VK_NULL_HANDLE));
    m_submissions.clear();
    return m_submittedValue;
}

VkSemaphore RendererFrame::getImageAvailableSemaphoreHandle() const {
    return m_imageAvailableSemaphore;
}

VkSemaphore RendererFrame::getRenderFinishedSemaphoreHandle() const {
    return m_renderFinishedSemaphore;
}

} // namespace crisp
