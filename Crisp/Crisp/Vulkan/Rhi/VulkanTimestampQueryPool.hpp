#pragma once

#include <span>
#include <string_view>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

namespace crisp {

class VulkanTimestampQueryPool {
public:
    VulkanTimestampQueryPool(
        const VulkanDevice& device, const VulkanQueue& queue, uint32_t queryCount, std::string_view debugName = {});
    ~VulkanTimestampQueryPool();

    VulkanTimestampQueryPool(const VulkanTimestampQueryPool&) = delete;
    VulkanTimestampQueryPool& operator=(const VulkanTimestampQueryPool&) = delete;
    VulkanTimestampQueryPool(VulkanTimestampQueryPool&&) = delete;
    VulkanTimestampQueryPool& operator=(VulkanTimestampQueryPool&&) = delete;

    void reset() const;
    void writeTimestamp(VkCommandBuffer cmdBuffer, VkPipelineStageFlags2 stage, uint32_t queryIndex) const;

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
    VkQueryPool m_handle{VK_NULL_HANDLE};
    uint32_t m_queryCount;
    uint64_t m_timestampMask;
    double m_timestampPeriodNs;
};

} // namespace crisp
