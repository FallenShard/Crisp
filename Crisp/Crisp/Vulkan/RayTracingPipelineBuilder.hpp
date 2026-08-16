#pragma once

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

#include <array>
#include <filesystem>
#include <memory>
#include <unordered_map>
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
    explicit RayTracingPipelineBuilder(VulkanDevice& device);
    ~RayTracingPipelineBuilder();

    RayTracingPipelineBuilder(const RayTracingPipelineBuilder&) = delete;
    RayTracingPipelineBuilder& operator=(const RayTracingPipelineBuilder&) = delete;
    RayTracingPipelineBuilder(RayTracingPipelineBuilder&&) = delete;
    RayTracingPipelineBuilder& operator=(RayTracingPipelineBuilder&&) = delete;

    void addShaderStage(const std::filesystem::path& spvPath);
    void addShaderGroup(uint32_t shaderStageIdx, VkRayTracingShaderGroupTypeKHR type);

    VkPipeline createHandle(VkPipelineLayout pipelineLayout);
    ShaderBindingTable createShaderBindingTable(VkPipeline rayTracingPipeline);

private:
    VulkanDevice& m_device;

    std::vector<VkShaderModule> m_shaderModules;
    std::vector<VkPipelineShaderStageCreateInfo> m_stages;
    std::unordered_map<VkShaderStageFlagBits, int32_t> m_stageCounts;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_groups;
};

} // namespace crisp
