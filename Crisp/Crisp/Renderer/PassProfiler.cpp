#include <Crisp/Renderer/PassProfiler.hpp>

#include <Crisp/Core/Format.hpp>

namespace crisp {

void PassProfiler::initialize(const VulkanDevice& device, const size_t passCount, const std::string_view debugName) {
    const uint32_t requiredQueryCount = static_cast<uint32_t>(passCount) * 2;
    const uint32_t timestampValidBits = device.getGeneralQueue().getTimestampValidBits();

    if (m_device != &device || m_queryCount != requiredQueryCount || timestampValidBits == 0) {
        m_frames.clear();
    }

    m_device = &device;
    m_debugName = debugName;
    m_queryCount = timestampValidBits == 0 ? 0 : requiredQueryCount;
    m_passTimingsMs.assign(passCount, std::nullopt);
    m_totalTimingMs.reset();
    m_currentFrame = nullptr;

    for (auto& frame : m_frames) {
        frame.queryPool->reset();
        frame.pending = false;
    }
}

void PassProfiler::beginFrame(const uint32_t virtualFrameIndex) {
    m_currentFrame = nullptr;
    if (!m_device || m_queryCount == 0) {
        return;
    }

    while (m_frames.size() <= virtualFrameIndex) {
        auto& frame = m_frames.emplace_back();
        frame.timestamps.resize(m_queryCount);
        frame.queryPool = std::make_unique<VulkanTimestampQueryPool>(
            *m_device,
            m_device->getGeneralQueue(),
            m_queryCount,
            fmt::format("{} GPU Queries {}", m_debugName, m_frames.size() - 1));
    }

    auto& frame = m_frames[virtualFrameIndex];
    if (!frame.pending) {
        m_currentFrame = &frame;
        return;
    }

    // The caller has already retired this virtual frame's previous submission.
    if (!frame.queryPool->tryGetResults(frame.timestamps)) {
        return;
    }

    for (size_t passIndex = 0; passIndex < m_passTimingsMs.size(); ++passIndex) {
        const uint64_t begin = frame.timestamps[passIndex * 2];
        const uint64_t end = frame.timestamps[passIndex * 2 + 1];
        m_passTimingsMs[passIndex] = frame.queryPool->getElapsedMilliseconds(begin, end);
    }
    m_totalTimingMs = frame.queryPool->getElapsedMilliseconds(frame.timestamps.front(), frame.timestamps.back());

    frame.queryPool->reset();
    frame.pending = false;
    m_currentFrame = &frame;
}

void PassProfiler::endFrame() {
    if (m_currentFrame) {
        m_currentFrame->pending = true;
        m_currentFrame = nullptr;
    }
}

void PassProfiler::beginPass(const VulkanCommandEncoder& encoder, const uint32_t passIndex) const {
    if (m_currentFrame) {
        encoder.writeTimestamp(*m_currentFrame->queryPool, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, passIndex * 2);
    }
}

void PassProfiler::endPass(const VulkanCommandEncoder& encoder, const uint32_t passIndex) const {
    if (m_currentFrame) {
        encoder.writeTimestamp(*m_currentFrame->queryPool, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, passIndex * 2 + 1);
    }
}

} // namespace crisp
