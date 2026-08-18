#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanTimestampQueryPool.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

namespace crisp {

// Timestamps a fixed number of passes with one query pool per virtual frame. Results are read back
// non-blocking, so the reported timings lag the submission by up to NumVirtualFrames.
class PassProfiler {
public:
    void initialize(const VulkanDevice& device, size_t passCount, std::string_view debugName);

    // Publishes the timings of this virtual frame's previous submission and opens a new recording.
    void beginFrame(uint32_t virtualFrameIndex);
    void endFrame();

    void beginPass(const VulkanCommandEncoder& encoder, uint32_t passIndex) const;
    void endPass(const VulkanCommandEncoder& encoder, uint32_t passIndex) const;

    bool isSupported() const {
        return m_queryCount > 0;
    }

    std::span<const std::optional<double>> getPassTimingsMs() const {
        return m_passTimingsMs;
    }

    // First pass's begin to last pass's end, including any gaps between them.
    std::optional<double> getTotalTimingMs() const {
        return m_totalTimingMs;
    }

private:
    struct Frame {
        std::unique_ptr<VulkanTimestampQueryPool> queryPool;
        std::vector<uint64_t> timestamps;
        bool pending{false};
    };

    const VulkanDevice* m_device{nullptr};
    std::string m_debugName;
    std::vector<Frame> m_frames;
    std::vector<std::optional<double>> m_passTimingsMs;
    std::optional<double> m_totalTimingMs;
    uint32_t m_queryCount{0};

    // The frame currently being recorded into, if any.
    Frame* m_currentFrame{nullptr};
};

} // namespace crisp
