#pragma once

#include <Crisp/Renderer/Renderer.hpp>

#include <vector>

namespace crisp {

struct ShaderBindingTable {
    static constexpr int32_t kRayGen = 0;
    static constexpr int32_t kMiss = 1;
    static constexpr int32_t kHit = 2;
    static constexpr int32_t kCall = 3;

    std::unique_ptr<VulkanBuffer> buffer;

    std::array<VkStridedDeviceAddressRegionKHR, 4> bindings;
};

class RayTracingPipelineBuilder {
public:
    explicit RayTracingPipelineBuilder(Renderer& renderer);

    void addShaderStage(const std::string& shaderName);
    void addShaderGroup(uint32_t shaderStageIdx, VkRayTracingShaderGroupTypeKHR type);

    VkPipeline createHandle(VkPipelineLayout pipelineLayout);
    ShaderBindingTable createShaderBindingTable(VkPipeline rayTracingPipeline);

private:
    Renderer& m_renderer;

    std::vector<VkPipelineShaderStageCreateInfo> m_stages;
    std::unordered_map<VkShaderStageFlagBits, int32_t> m_stageCounts;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_groups;
};

} // namespace crisp