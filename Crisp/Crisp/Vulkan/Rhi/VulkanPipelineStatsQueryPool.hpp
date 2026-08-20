#pragma once

#include <bitset>
#include <optional>
#include <string_view>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {

struct PipelineStats {
    std::optional<uint64_t> inputAssemblyVertices;
    std::optional<uint64_t> inputAssemblyPrimitives;
    std::optional<uint64_t> vertexShaderInvocations;
    std::optional<uint64_t> geometryShaderInvocations;
    std::optional<uint64_t> geometryShaderPrimitives;
    std::optional<uint64_t> clippingInvocations;
    std::optional<uint64_t> clippingPrimitives;
    std::optional<uint64_t> fragmentShaderInvocations;
    std::optional<uint64_t> tessellationControlShaderPatches;
    std::optional<uint64_t> tessellationEvaluationShaderInvocations;
    std::optional<uint64_t> computeShaderInvocations;
};

class VulkanPipelineStatsQueryPool : public VulkanResource<VkQueryPool> {
public:
    VulkanPipelineStatsQueryPool(
        const VulkanDevice& device,
        VkQueryPipelineStatisticFlags statistics,
        uint32_t queryCount = 1,
        std::string_view debugName = {});

    void reset();
    void reset(uint32_t queryIndex);

    bool tryGetResults(PipelineStats& stats, uint32_t queryIndex = 0) const;

    uint32_t getStatisticCount() const {
        return m_statisticCount;
    }

    bool isPending(const uint32_t queryIndex) const {
        return m_pending[queryIndex];
    }

    void setPending(const uint32_t queryIndex) {
        m_pending.set(queryIndex);
    }

private:
    const VulkanDevice* m_device;
    uint32_t m_queryCount;
    uint32_t m_statisticCount;
    VkQueryPipelineStatisticFlags m_statistics;
    std::bitset<32> m_pending;
};

} // namespace crisp
