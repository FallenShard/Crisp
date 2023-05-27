#pragma once

#include <Crisp/Utils/BitFlags.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFormatTraits.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>

#include <memory>
#include <vector>

namespace crisp
{
enum class PipelineState
{
    VertexInput = 0x001,
    InputAssembly = 0x002,
    Tessellation = 0x004,
    Viewport = 0x008,
    Rasterization = 0x010,
    Multisample = 0x020,
    ColorBlend = 0x040,
    DepthStencil = 0x080,
    Dynamic = 0x100,

    Default = VertexInput | InputAssembly | Viewport | Rasterization | Multisample | ColorBlend | DepthStencil
};
DECLARE_BITFLAG(PipelineState);

class PipelineBuilder
{
public:
    PipelineBuilder();

    PipelineBuilder& addShaderStage(VkPipelineShaderStageCreateInfo&& shaderStage);
    PipelineBuilder& setShaderStages(std::initializer_list<VkPipelineShaderStageCreateInfo> shaderStages);
    PipelineBuilder& setShaderStages(std::vector<VkPipelineShaderStageCreateInfo>&& shaderStages);

    PipelineBuilder& addVertexInputBinding(
        uint32_t binding, VkVertexInputRate inputRate, const std::vector<VkFormat>& formats);
    PipelineBuilder& addVertexAttributes(uint32_t binding, const std::vector<VkFormat>& formats);

    PipelineBuilder& setFullScreenVertexLayout();

    PipelineBuilder& setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);
    PipelineBuilder& setTessellationControlPoints(uint32_t numControlPoints);

    PipelineBuilder& setPolygonMode(VkPolygonMode polygonMode);
    PipelineBuilder& setFrontFace(VkFrontFace frontFace);
    PipelineBuilder& setCullMode(VkCullModeFlags cullMode);
    PipelineBuilder& setLineWidth(float lineWidth);

    PipelineBuilder& setSampleCount(VkSampleCountFlagBits sampleCount);
    PipelineBuilder& setAlphaToCoverage(VkBool32 alphaToCoverageEnabled);

    PipelineBuilder& setViewport(VkViewport&& viewport);
    PipelineBuilder& setScissor(VkRect2D&& scissor);

    PipelineBuilder& setBlendState(uint32_t index, VkBool32 enabled);
    PipelineBuilder& setBlendFactors(uint32_t index, VkBlendFactor srcFactor, VkBlendFactor dstFactor);

    PipelineBuilder& setDepthTest(VkBool32 enabled);
    PipelineBuilder& setDepthTestOperation(VkCompareOp testOperation);
    PipelineBuilder& setDepthWrite(VkBool32 enabled);

    PipelineBuilder& enableState(PipelineState pipelineState);
    PipelineBuilder& disableState(PipelineState pipelineState);

    PipelineBuilder& addDynamicState(VkDynamicState dynamicState);

    std::unique_ptr<VulkanPipeline> create(
        const VulkanDevice& device,
        std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
        VkRenderPass renderPass,
        uint32_t subpassIndex);
    PipelineDynamicStateFlags createDynamicStateFlags() const;

private:
    VkPipelineRasterizationStateCreateInfo createDefaultRasterizationState();
    VkPipelineMultisampleStateCreateInfo createDefaultMultisampleState();
    VkPipelineColorBlendAttachmentState createDefaultColorBlendAttachmentState();
    VkPipelineColorBlendStateCreateInfo createDefaultColorBlendState();
    VkPipelineDepthStencilStateCreateInfo createDefaultDepthStencilState();

    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

    VertexLayout m_vertexLayout;
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

    PipelineStateFlags m_pipelineStateFlags;
};

VkPipelineShaderStageCreateInfo createShaderStageInfo(
    VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint = "main");
} // namespace crisp