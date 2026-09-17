#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRasterizationPassDescriptor.hpp>

namespace crisp {

struct DescriptorHeapStageMappings {
    VkShaderStageFlagBits stage;
    std::vector<VkDescriptorSetAndBindingMappingEXT> mappings;
};

struct DescriptorHeapPipelineParams {
    std::vector<DescriptorHeapStageMappings> stageMappings;
};

struct VulkanPipelineParams {
    VulkanRasterizationPassDescriptor rasterizationPassDescriptor;
    SpecializationConstantMap specializationConstants;
    std::optional<DescriptorHeapPipelineParams> descriptorHeapParams;
};

class PipelineBuilder {
public:
    PipelineBuilder();

    PipelineBuilder& addShaderStage(const VkPipelineShaderStageCreateInfo& shaderStage);
    PipelineBuilder& setShaderStages(std::span<const VkPipelineShaderStageCreateInfo> shaderStages);
    PipelineBuilder& setShaderStages(std::vector<VkPipelineShaderStageCreateInfo>&& shaderStages);

    PipelineBuilder& addVertexInputBinding(
        uint32_t binding, VkVertexInputRate inputRate, std::span<const VkFormat> formats);
    PipelineBuilder& addVertexAttributes(uint32_t binding, std::span<const VkFormat> formats);
    PipelineBuilder& addVertexAttributes(
        uint32_t binding, std::span<const uint32_t> locations, std::span<const VkFormat> formats);

    PipelineBuilder& setFullScreenVertexLayout();

    PipelineBuilder& setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);
    PipelineBuilder& setTessellationControlPoints(uint32_t numControlPoints);

    PipelineBuilder& setPolygonMode(VkPolygonMode polygonMode);
    PipelineBuilder& setFrontFace(VkFrontFace frontFace);
    PipelineBuilder& setCullMode(VkCullModeFlags cullMode);
    PipelineBuilder& setLineWidth(float lineWidth);
    PipelineBuilder& setDepthBias(float constantFactor, float slopeFactor, float clamp = 0.0f);

    PipelineBuilder& setSampleCount(VkSampleCountFlagBits sampleCount);
    PipelineBuilder& setAlphaToCoverage(VkBool32 alphaToCoverageEnabled);

    PipelineBuilder& setViewport(const VkViewport& viewport);
    PipelineBuilder& setScissor(const VkRect2D& scissor);

    PipelineBuilder& setBlendState(uint32_t index, VkBool32 enabled);
    PipelineBuilder& setBlendFactors(uint32_t index, VkBlendFactor srcFactor, VkBlendFactor dstFactor);

    PipelineBuilder& setDepthTest(VkBool32 enabled);
    PipelineBuilder& setDepthTestOperation(VkCompareOp testOperation);
    PipelineBuilder& setDepthWrite(VkBool32 enabled);

    PipelineBuilder& addDynamicState(PipelineDynamicState dynamicState);
    PipelineBuilder& addDynamicStates(PipelineDynamicStateFlags dynamicStates);
    PipelineBuilder& setDescriptorHeapMappings(
        uint32_t shaderStageIdx, std::span<const VkDescriptorSetAndBindingMappingEXT> mappings);

    size_t getShaderStageCount() const {
        return m_shaderStages.size();
    }

    std::unique_ptr<VulkanPipeline> create(
        const VulkanDevice& device,
        std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
        const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor);
    std::unique_ptr<VulkanPipeline> createDescriptorHeap(
        const VulkanDevice& device, const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor);

    PipelineDynamicStateFlags getDynamicStateFlags() const {
        return m_dynamicStateFlags;
    }

private:
    std::unique_ptr<VulkanPipeline> createImpl(
        const VulkanDevice& device,
        std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
        const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor,
        bool descriptorHeap);
    void populatePipelineCreateInfo(VkGraphicsPipelineCreateInfo& pipelineInfo, VkPipelineLayout pipelineLayout) const;

    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

    struct StageMappings {
        std::vector<VkDescriptorSetAndBindingMappingEXT> mappings;
        VkShaderDescriptorSetAndBindingMappingInfoEXT info{
            VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT};
    };

    std::vector<std::unique_ptr<StageMappings>> m_stageMappings;

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

    PipelineDynamicStateFlags m_dynamicStateFlags;
};

std::vector<VkDynamicState> toVkDynamicStates(PipelineDynamicStateFlags flags);

PipelineDynamicState parsePipelineDynamicState(std::string_view name);

std::string_view toString(PipelineDynamicState dynamicState);

VkPipelineShaderStageCreateInfo createShaderStageInfo(
    VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint = "main");
} // namespace crisp
