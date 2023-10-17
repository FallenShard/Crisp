#pragma once

#include <Crisp/Renderer/Renderer.hpp>

#include <vector>

namespace crisp {

class RayTracingPipelineBuilder {
public:
    explicit RayTracingPipelineBuilder(Renderer& renderer);

    void addShaderStage(const std::string& shaderName);
    void addShaderGroup(uint32_t shaderStageIdx, VkRayTracingShaderGroupTypeKHR type);

    VkPipeline createHandle(VkPipelineLayout pipelineLayout);
    std::unique_ptr<VulkanBuffer> createShaderBindingTable(VkPipeline rayTracingPipeline);

private:
    Renderer& m_renderer;

    std::vector<VkPipelineShaderStageCreateInfo> m_stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_groups;
};

} // namespace crisp