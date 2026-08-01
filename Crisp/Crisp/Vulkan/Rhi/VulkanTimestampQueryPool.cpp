#include <Crisp/Vulkan/Rhi/VulkanTimestampQueryPool.hpp>

#include <limits>
#include <string>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {

uint64_t createTimestampMask(const uint32_t timestampValidBits) {
    CRISP_CHECK_GT(timestampValidBits, 0);
    CRISP_CHECK_LE(timestampValidBits, 64);
    return timestampValidBits == 64 ? std::numeric_limits<uint64_t>::max() : (uint64_t{1} << timestampValidBits) - 1;
}

} // namespace

VulkanTimestampQueryPool::VulkanTimestampQueryPool(
    const VulkanDevice& device, const VulkanQueue& queue, const uint32_t queryCount, const std::string_view debugName)
    : m_device(&device)
    , m_queryCount(queryCount)
    , m_timestampMask(createTimestampMask(queue.getTimestampValidBits()))
    , m_timestampPeriodNs(device.getTimestampPeriod()) {
    CRISP_CHECK_GT(queryCount, 0);

    const VkQueryPoolCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = queryCount,
    };
    VK_FATAL(vkCreateQueryPool(device.getHandle(), &createInfo, nullptr, &m_handle));
    if (!debugName.empty()) {
        device.setObjectName(m_handle, std::string(debugName));
    }
    reset();
}

VulkanTimestampQueryPool::~VulkanTimestampQueryPool() {
    m_device->getResourceDeallocator().deferDestruction(m_handle);
}

void VulkanTimestampQueryPool::reset() const {
    vkResetQueryPool(m_device->getHandle(), m_handle, 0, m_queryCount);
}

void VulkanTimestampQueryPool::writeTimestamp(
    const VkCommandBuffer cmdBuffer, const VkPipelineStageFlags2 stage, const uint32_t queryIndex) const {
    CRISP_CHECK_LT(queryIndex, m_queryCount);
    vkCmdWriteTimestamp2(cmdBuffer, stage, m_handle, queryIndex);
}

bool VulkanTimestampQueryPool::tryGetResults(const std::span<uint64_t> results, const uint32_t firstQuery) const {
    const VkResult result = getResults(results, firstQuery, VK_QUERY_RESULT_64_BIT);
    if (result == VK_NOT_READY) {
        return false;
    }
    VK_FATAL(result);
    return true;
}

void VulkanTimestampQueryPool::getResultsAndWait(const std::span<uint64_t> results, const uint32_t firstQuery) const {
    VK_FATAL(getResults(results, firstQuery, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
}

double VulkanTimestampQueryPool::toNanoseconds(const uint64_t timestamp) const {
    return static_cast<double>(timestamp & m_timestampMask) * m_timestampPeriodNs;
}

double VulkanTimestampQueryPool::getElapsedMilliseconds(const uint64_t begin, const uint64_t end) const {
    return static_cast<double>(getElapsedTicks(begin, end)) * m_timestampPeriodNs / 1'000'000.0;
}

VkResult VulkanTimestampQueryPool::getResults(
    const std::span<uint64_t> results, const uint32_t firstQuery, const VkQueryResultFlags flags) const {
    CRISP_CHECK(!results.empty());
    CRISP_CHECK_LE(firstQuery, m_queryCount);
    CRISP_CHECK_LE(results.size(), m_queryCount - firstQuery);
    return vkGetQueryPoolResults(
        m_device->getHandle(),
        m_handle,
        firstQuery,
        static_cast<uint32_t>(results.size()),
        results.size_bytes(),
        results.data(),
        sizeof(uint64_t),
        flags);
}

uint64_t VulkanTimestampQueryPool::getElapsedTicks(const uint64_t begin, const uint64_t end) const {
    return (end - begin) & m_timestampMask;
}

} // namespace crisp
