#pragma once

#include <string_view>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {

class VulkanTimelineSemaphore final : public VulkanResource<VkSemaphore> {
public:
    explicit VulkanTimelineSemaphore(
        const VulkanDevice& device, uint64_t initialValue = 0, std::string_view debugName = {});

    // Reserves and returns the value that the next submission should signal. CPU-side only: it says what has been
    // scheduled, not what the GPU has reached. Use getCompletedValue() for the latter.
    uint64_t advance();

    uint64_t getScheduledValue() const {
        return m_scheduledValue;
    }

    uint64_t getCompletedValue() const;

    bool isComplete(const uint64_t value) const {
        return getCompletedValue() >= value;
    }

    void wait(uint64_t value) const;

    bool wait(uint64_t value, uint64_t timeoutNs) const;

    // Signals from the host, without a submission. Value must exceed the current counter.
    void signal(uint64_t value);

private:
    uint64_t m_scheduledValue;
};

} // namespace crisp
