#include <Crisp/Renderer/RendererFrame.hpp>

namespace crisp
{
RendererFrame::RendererFrame(const VulkanDevice& device, const int32_t logicalIndex)
    : m_completionFence(device.createFence(0))
    , m_imageAvailableSemaphore(device.createSemaphore())
    , m_renderFinishedSemaphore(device.createSemaphore())
    , m_deviceHandle(device.getHandle())
    , m_logicalIndex(logicalIndex)
{
    device.getDebugMarker().setObjectName(
        m_completionFence, fmt::format("[Frame {}] Frame Completion Fence", m_logicalIndex));
    device.getDebugMarker().setObjectName(
        m_imageAvailableSemaphore, fmt::format("[Frame {}] Image Available Sem", m_logicalIndex));
    device.getDebugMarker().setObjectName(
        m_renderFinishedSemaphore, fmt::format("[Frame {}] Render Finished Sem", m_logicalIndex));
}

RendererFrame::~RendererFrame()
{
    if (m_completionFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_deviceHandle, m_completionFence, nullptr);
    }
    if (m_imageAvailableSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_deviceHandle, m_imageAvailableSemaphore, nullptr);
    }
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE)
    {
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
    , m_submissions(std::move(other.m_submissions))
{
}

RendererFrame& RendererFrame::operator=(RendererFrame&& other) noexcept
{
    if (this == &other)
    {
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

void RendererFrame::waitCompletion(VkDevice device)
{
    if (m_status == Status::Idle)
    {
        return;
    }

    vkWaitForFences(device, 1, &m_completionFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkResetFences(device, 1, &m_completionFence);
    m_status = Status::Idle;
}

void RendererFrame::addSubmission(const VulkanCommandBuffer& cmdBuffer)
{
    Submission submission{};
    submission.cmdBufferHandles.push_back(cmdBuffer.getHandle());
    submission.waitSemaphores.push_back(m_imageAvailableSemaphore);
    submission.waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    submission.signalSemaphores.push_back(m_renderFinishedSemaphore);
    m_submissions.push_back(submission);
}

void RendererFrame::submitToQueue(const VulkanQueue& queue)
{
    std::vector<VkSubmitInfo> submitInfos{};
    submitInfos.reserve(m_submissions.size());
    for (const auto& submission : m_submissions)
    {
        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(submission.waitSemaphores.size());
        submitInfo.pWaitSemaphores = submission.waitSemaphores.data();
        submitInfo.pWaitDstStageMask = submission.waitStages.data();
        submitInfo.commandBufferCount = static_cast<uint32_t>(submission.cmdBufferHandles.size());
        submitInfo.pCommandBuffers = submission.cmdBufferHandles.data();
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(submission.signalSemaphores.size());
        submitInfo.pSignalSemaphores = submission.signalSemaphores.data();
        submitInfos.push_back(submitInfo);
    }
    vkQueueSubmit(queue.getHandle(), static_cast<uint32_t>(submitInfos.size()), submitInfos.data(), m_completionFence);
    m_submissions.clear();
    m_status = Status::Submitted;
}

VkSemaphore RendererFrame::getImageAvailableSemaphoreHandle() const
{
    return m_imageAvailableSemaphore;
}

VkSemaphore RendererFrame::getRenderFinishedSemaphoreHandle() const
{
    return m_renderFinishedSemaphore;
}

} // namespace crisp
