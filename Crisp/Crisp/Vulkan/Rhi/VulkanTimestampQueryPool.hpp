#pragma once

#include <span>
#include <string_view>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {

class VulkanTimestampQueryPool : public VulkanResource<VkQueryPool> {
public:
    VulkanTimestampQueryPool(
        const VulkanDevice& device, const VulkanQueue& queue, uint32_t queryCount, std::string_view debugName = {});

    void reset() const;

    bool tryGetResults(std::span<uint64_t> results, uint32_t firstQuery = 0) const;
    void getResultsAndWait(std::span<uint64_t> results, uint32_t firstQuery = 0) const;

    double toNanoseconds(uint64_t timestamp) const;
    double getElapsedMilliseconds(uint64_t begin, uint64_t end) const;

    uint32_t getQueryCount() const {
        return m_queryCount;
    }

private:
    VkResult getResults(std::span<uint64_t> results, uint32_t firstQuery, VkQueryResultFlags flags) const;
    uint64_t getElapsedTicks(uint64_t begin, uint64_t end) const;

    const VulkanDevice* m_device;
    uint32_t m_queryCount;
    uint64_t m_timestampMask;
    double m_timestampPeriodNs;
};

} // namespace crisp
