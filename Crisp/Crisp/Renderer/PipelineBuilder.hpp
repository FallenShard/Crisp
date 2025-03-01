#pragma once

#include <memory>
#include <span>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

namespace crisp {

class PipelineBuilder {
public:
    PipelineBuilder();

    PipelineBuilder& addShaderStage(const VkPipelineShaderStageCreateInfo& shaderStage);
    PipelineBuilder& setShaderStages(std::span<const VkPipelineShaderStageCreateInfo> shaderStages);
    PipelineBuilder& setShaderStages(std::vector<VkPipelineShaderStageCreateInfo>&& shaderStages);

    PipelineBuilder& addVertexInputBinding(
        uint32_t binding, VkVertexInputRate inputRate, std::span<const VkFormat> formats);
    PipelineBuilder& addVertexAttributes(uint32_t binding, std::span<const VkFormat> formats);

    PipelineBuilder& setFullScreenVertexLayout();

    PipelineBuilder& setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);
    PipelineBuilder& setTessellationControlPoints(uint32_t numControlPoints);

    PipelineBuilder& setPolygonMode(VkPolygonMode polygonMode);
    PipelineBuilder& setFrontFace(VkFrontFace frontFace);
    PipelineBuilder& setCullMode(VkCullModeFlags cullMode);
    PipelineBuilder& setLineWidth(float lineWidth);

    PipelineBuilder& setSampleCount(VkSampleCountFlagBits sampleCount);
    PipelineBuilder& setAlphaToCoverage(VkBool32 alphaToCoverageEnabled);

    PipelineBuilder& setViewport(const VkViewport& viewport);
    PipelineBuilder& setScissor(const VkRect2D& scissor);

    PipelineBuilder& setBlendState(uint32_t index, VkBool32 enabled);
    PipelineBuilder& setBlendFactors(uint32_t index, VkBlendFactor srcFactor, VkBlendFactor dstFactor);

    PipelineBuilder& setDepthTest(VkBool32 enabled);
    PipelineBuilder& setDepthTestOperation(VkCompareOp testOperation);
    PipelineBuilder& setDepthWrite(VkBool32 enabled);

    PipelineBuilder& addDynamicState(VkDynamicState dynamicState);

    std::unique_ptr<VulkanPipeline> create(
        const VulkanDevice& device,
        std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
        VkRenderPass renderPass,
        uint32_t subpassIndex);
    PipelineDynamicStateFlags createDynamicStateFlags() const;

private:
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

    VulkanVertexLayout m_vertexLayout;
    VkPipelineVertexInputStateCreateInfo m_vertexInputState;

    VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyState;

    VkPipelineTessellationStateCreateInfo m_tessellationState;

    std::vector<VkViewport> m_viewports;
    std::vector<VkRect2D> m_scissors;
    VkPipelineViewportStateCreateInfo m_viewportState;

    VkPipelineRasterizationStateCreateInfo m_rasterizationState;

    VkPipelineMultisampleStateCreateInfo m_multisampleState;

    std::vector<VkPipelineColorBlendAttachmentState> m_colorBlendAttachmentStates;
    VkPipelineColorBlendStateCreateInfo m_colorBlendState;

    VkPipelineDepthStencilStateCreateInfo m_depthStencilState;

    std::vector<VkDynamicState> m_dynamicStates;
    VkPipelineDynamicStateCreateInfo m_dynamicState;
};

VkPipelineShaderStageCreateInfo createShaderStageInfo(
    VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint = "main");
} // namespace crisp