#include <Crisp/Vulkan/Rhi/VulkanPipelineStatsQueryPool.hpp>

#include <array>
#include <bit>
#include <string>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {

constexpr std::array kStatFields{
    &PipelineStats::inputAssemblyVertices,
    &PipelineStats::inputAssemblyPrimitives,
    &PipelineStats::vertexShaderInvocations,
    &PipelineStats::geometryShaderInvocations,
    &PipelineStats::geometryShaderPrimitives,
    &PipelineStats::clippingInvocations,
    &PipelineStats::clippingPrimitives,
    &PipelineStats::fragmentShaderInvocations,
    &PipelineStats::tessellationControlShaderPatches,
    &PipelineStats::tessellationEvaluationShaderInvocations,
    &PipelineStats::computeShaderInvocations,
};

} // namespace

VulkanPipelineStatsQueryPool::VulkanPipelineStatsQueryPool(
    const VulkanDevice& device,
    const VkQueryPipelineStatisticFlags statistics,
    const uint32_t queryCount,
    const std::string_view debugName)
    : VulkanResource(device.getResourceDeallocator())
    , m_device(&device)
    , m_queryCount(queryCount)
    , m_statisticCount(static_cast<uint32_t>(std::popcount(statistics)))
    , m_statistics(statistics) {
    CRISP_CHECK_GT(queryCount, 0);
    CRISP_CHECK_GT(m_statisticCount, 0);
    CRISP_CHECK_LE(queryCount, m_pending.size());
    // The packed readback buffer below is sized for the statistics kStatFields names; a wider mask (the task and
    // mesh shader bits) would run off the end of it.
    CRISP_CHECK_LE(m_statisticCount, kStatFields.size());

    const VkQueryPoolCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
        .queryCount = queryCount,
        .pipelineStatistics = statistics,
    };
    VK_FATAL(vkCreateQueryPool(device.getHandle(), &createInfo, nullptr, &m_handle));
    if (!debugName.empty()) {
        device.setObjectName(m_handle, std::string(debugName));
    }
    reset();
}

void VulkanPipelineStatsQueryPool::reset() {
    vkResetQueryPool(m_device->getHandle(), m_handle, 0, m_queryCount);
    m_pending.reset();
}

void VulkanPipelineStatsQueryPool::reset(const uint32_t queryIndex) {
    CRISP_CHECK_LT(queryIndex, m_queryCount);
    vkResetQueryPool(m_device->getHandle(), m_handle, queryIndex, 1);
    m_pending.reset(queryIndex);
}

bool VulkanPipelineStatsQueryPool::tryGetResults(PipelineStats& stats, const uint32_t queryIndex) const {
    CRISP_CHECK_LT(queryIndex, m_queryCount);

    std::array<uint64_t, kStatFields.size()> packed{};
    const VkDeviceSize stride = static_cast<VkDeviceSize>(m_statisticCount) * sizeof(uint64_t);
    const VkResult result = vkGetQueryPoolResults(
        m_device->getHandle(), m_handle, queryIndex, 1, stride, packed.data(), stride, VK_QUERY_RESULT_64_BIT);
    if (result == VK_NOT_READY) {
        return false;
    }
    VK_FATAL(result);

    size_t packedIdx = 0;
    for (size_t bit = 0; bit < kStatFields.size(); ++bit) {
        if ((m_statistics & (VkQueryPipelineStatisticFlags{1} << bit)) != 0) {
            stats.*kStatFields[bit] = packed[packedIdx++];
        }
    }
    return true;
}

} // namespace crisp
