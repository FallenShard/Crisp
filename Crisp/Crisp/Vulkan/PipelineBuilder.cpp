#include <Crisp/Vulkan/PipelineBuilder.hpp>

#include <array>
#include <ranges>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {

enum class VertexAttributeLayout : uint8_t { Interleaved, Concatenated };

std::vector<VkVertexInputAttributeDescription> generateVertexInputAttributes(
    const uint32_t binding,
    const std::span<const uint32_t> locations,
    const std::span<const VkFormat> formats,
    const VertexAttributeLayout layout) {
    CRISP_CHECK_SIZE_EQ(locations, formats);
    std::vector<VkVertexInputAttributeDescription> vertexAttribs(formats.size());

    uint32_t offset = 0;
    for (uint32_t i = 0; i < vertexAttribs.size(); ++i) {
        vertexAttribs[i].location = locations[i];
        vertexAttribs[i].binding = binding;
        vertexAttribs[i].format = formats[i];
        vertexAttribs[i].offset = layout == VertexAttributeLayout::Interleaved ? offset : 0;
        offset += getSizeOf(formats[i]);
    }

    return vertexAttribs;
}

struct DynamicStateMapping {
    PipelineDynamicState state;
    VkDynamicState vkState;
    std::string_view name; // Spelling accepted by the "dynamicState" array in pipeline JSON.
};

// The one place the three spellings of a dynamic state are tied together, so they cannot drift apart.
constexpr std::array kDynamicStateMappings{
    DynamicStateMapping{PipelineDynamicState::Viewport, VK_DYNAMIC_STATE_VIEWPORT, "viewport"},
    DynamicStateMapping{PipelineDynamicState::Scissor, VK_DYNAMIC_STATE_SCISSOR, "scissor"},
    DynamicStateMapping{PipelineDynamicState::LineWidth, VK_DYNAMIC_STATE_LINE_WIDTH, "lineWidth"},
    DynamicStateMapping{PipelineDynamicState::DepthBias, VK_DYNAMIC_STATE_DEPTH_BIAS, "depthBias"},
    DynamicStateMapping{PipelineDynamicState::BlendConstants, VK_DYNAMIC_STATE_BLEND_CONSTANTS, "blendConstants"},
    DynamicStateMapping{PipelineDynamicState::DepthBounds, VK_DYNAMIC_STATE_DEPTH_BOUNDS, "depthBounds"},
    DynamicStateMapping{
        PipelineDynamicState::StencilCompareMask, VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK, "stencilCompareMask"},
    DynamicStateMapping{PipelineDynamicState::StencilWriteMask, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, "stencilWriteMask"},
    DynamicStateMapping{PipelineDynamicState::StencilReference, VK_DYNAMIC_STATE_STENCIL_REFERENCE, "stencilReference"},
    DynamicStateMapping{PipelineDynamicState::CullMode, VK_DYNAMIC_STATE_CULL_MODE, "cullMode"},
    DynamicStateMapping{PipelineDynamicState::FrontFace, VK_DYNAMIC_STATE_FRONT_FACE, "frontFace"},
    DynamicStateMapping{
        PipelineDynamicState::PrimitiveTopology, VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY, "primitiveTopology"},
    DynamicStateMapping{PipelineDynamicState::DepthTestEnable, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE, "depthTestEnable"},
    DynamicStateMapping{PipelineDynamicState::DepthWriteEnable, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE, "depthWriteEnable"},
    DynamicStateMapping{PipelineDynamicState::DepthCompareOp, VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, "depthCompareOp"},
    DynamicStateMapping{
        PipelineDynamicState::DepthBoundsTestEnable, VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE, "depthBoundsTestEnable"},
    DynamicStateMapping{
        PipelineDynamicState::StencilTestEnable, VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE, "stencilTestEnable"},
    DynamicStateMapping{PipelineDynamicState::StencilOp, VK_DYNAMIC_STATE_STENCIL_OP, "stencilOp"},
    DynamicStateMapping{
        PipelineDynamicState::RasterizerDiscardEnable,
        VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
        "rasterizerDiscardEnable"},
    DynamicStateMapping{PipelineDynamicState::DepthBiasEnable, VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE, "depthBiasEnable"},
    DynamicStateMapping{
        PipelineDynamicState::PrimitiveRestartEnable,
        VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
        "primitiveRestartEnable"},
};

VkPipelineRasterizationStateCreateInfo createDefaultRasterizationState() {
    VkPipelineRasterizationStateCreateInfo rasterizationState{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.depthBiasClamp = 0.0f;
    rasterizationState.depthBiasConstantFactor = 0.0f;
    rasterizationState.depthBiasSlopeFactor = 0.0f;
    rasterizationState.lineWidth = 1.0f;
    return rasterizationState;
}

VkPipelineMultisampleStateCreateInfo createDefaultMultisampleState() {
    VkPipelineMultisampleStateCreateInfo multisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.sampleShadingEnable = VK_FALSE;
    multisampleState.minSampleShading = 1.0f;
    multisampleState.pSampleMask = nullptr;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.alphaToOneEnable = VK_FALSE;
    return multisampleState;
}

VkPipelineColorBlendAttachmentState createDefaultColorBlendAttachmentState() {
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    return colorBlendAttachment;
}

VkPipelineColorBlendStateCreateInfo createDefaultColorBlendState() {
    VkPipelineColorBlendStateCreateInfo colorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    colorBlendState.blendConstants[0] = 0.0f;
    colorBlendState.blendConstants[1] = 0.0f;
    colorBlendState.blendConstants[2] = 0.0f;
    colorBlendState.blendConstants[3] = 0.0f;
    return colorBlendState;
}

VkPipelineDepthStencilStateCreateInfo createDefaultDepthStencilState() {
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = {};
    depthStencilState.back = {};
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;
    return depthStencilState;
}

} // namespace

PipelineBuilder::PipelineBuilder()
    : m_vertexInputState({VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO})
    , m_inputAssemblyState({VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO})
    , m_tessellationState({VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO})
    , m_viewportState({VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO})
    , m_rasterizationState(createDefaultRasterizationState())
    , m_multisampleState(createDefaultMultisampleState())
    , m_colorBlendAttachmentStates({createDefaultColorBlendAttachmentState()})
    , m_colorBlendState(createDefaultColorBlendState())
    , m_depthStencilState(createDefaultDepthStencilState()) {
    m_colorBlendState.attachmentCount = static_cast<uint32_t>(m_colorBlendAttachmentStates.size());
    m_colorBlendState.pAttachments = m_colorBlendAttachmentStates.data();

    m_inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_inputAssemblyState.primitiveRestartEnable = VK_FALSE;
}

PipelineBuilder& PipelineBuilder::addShaderStage(const VkPipelineShaderStageCreateInfo& shaderStage) {
    m_shaderStages.emplace_back(shaderStage);
    return *this;
}

PipelineBuilder& PipelineBuilder::setShaderStages(const std::span<const VkPipelineShaderStageCreateInfo> shaderStages) {
    m_shaderStages.assign(shaderStages.begin(), shaderStages.end());
    return *this;
}

PipelineBuilder& PipelineBuilder::setShaderStages(std::vector<VkPipelineShaderStageCreateInfo>&& shaderStages) {
    m_shaderStages = std::move(shaderStages);
    return *this;
}

PipelineBuilder& PipelineBuilder::addVertexInputBinding(
    const uint32_t binding, const VkVertexInputRate inputRate, const std::span<const VkFormat> formats) {
    uint32_t aggregateSize = 0;
    for (const auto format : formats) {
        aggregateSize += getSizeOf(format);
    }

    m_vertexLayout.bindings.push_back({binding, aggregateSize, inputRate});
    m_vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexLayout.bindings.size());
    m_vertexInputState.pVertexBindingDescriptions = m_vertexLayout.bindings.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::addVertexAttributes(const uint32_t binding, const std::span<const VkFormat> formats) {
    const auto locationOffset = static_cast<uint32_t>(m_vertexLayout.attributes.size());
    std::vector<uint32_t> locations(formats.size());
    for (uint32_t i = 0; i < locations.size(); ++i) {
        locations[i] = locationOffset + i;
    }
    return addVertexAttributes(binding, locations, formats);
}

PipelineBuilder& PipelineBuilder::addVertexAttributes(
    const uint32_t binding, const std::span<const uint32_t> locations, const std::span<const VkFormat> formats) {
    auto attribs = generateVertexInputAttributes(binding, locations, formats, VertexAttributeLayout::Interleaved);
    m_vertexLayout.attributes.insert(m_vertexLayout.attributes.end(), attribs.begin(), attribs.end());
    m_vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexLayout.attributes.size());
    m_vertexInputState.pVertexAttributeDescriptions = m_vertexLayout.attributes.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::setFullScreenVertexLayout() {
    m_vertexLayout.bindings.clear();
    addVertexInputBinding(0, VK_VERTEX_INPUT_RATE_VERTEX, std::array{VK_FORMAT_R32G32_SFLOAT});

    m_vertexLayout.attributes.clear();
    addVertexAttributes(0, std::array{VK_FORMAT_R32G32_SFLOAT});
    return *this;
}

PipelineBuilder& PipelineBuilder::setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable) {
    m_inputAssemblyState.topology = topology;
    m_inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;
    return *this;
}

PipelineBuilder& PipelineBuilder::setTessellationControlPoints(uint32_t numControlPoints) {
    m_tessellationState.patchControlPoints = numControlPoints;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPolygonMode(VkPolygonMode polygonMode) {
    m_rasterizationState.polygonMode = polygonMode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setFrontFace(VkFrontFace frontFace) {
    m_rasterizationState.frontFace = frontFace;
    return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(VkCullModeFlags cullMode) {
    m_rasterizationState.cullMode = cullMode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setLineWidth(float lineWidth) {
    m_rasterizationState.lineWidth = lineWidth;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthBias(const float constantFactor, const float slopeFactor, const float clamp) {
    m_rasterizationState.depthBiasEnable = VK_TRUE;
    m_rasterizationState.depthBiasConstantFactor = constantFactor;
    m_rasterizationState.depthBiasSlopeFactor = slopeFactor;
    m_rasterizationState.depthBiasClamp = clamp;
    return *this;
}

PipelineBuilder& PipelineBuilder::setSampleCount(VkSampleCountFlagBits sampleCount) {
    m_multisampleState.rasterizationSamples = sampleCount;
    return *this;
}

PipelineBuilder& PipelineBuilder::setAlphaToCoverage(VkBool32 alphaToCoverageEnabled) {
    m_multisampleState.alphaToCoverageEnable = alphaToCoverageEnabled;
    return *this;
}

PipelineBuilder& PipelineBuilder::setViewport(const VkViewport& viewport) {
    m_viewports = {viewport};
    m_viewportState.viewportCount = static_cast<uint32_t>(m_viewports.size());
    m_viewportState.pViewports = m_viewports.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::setScissor(const VkRect2D& scissor) {
    m_scissors = {scissor};
    m_viewportState.scissorCount = static_cast<uint32_t>(m_scissors.size());
    m_viewportState.pScissors = m_scissors.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::setBlendState(uint32_t index, VkBool32 enabled) {
    if (index >= m_colorBlendAttachmentStates.size()) {
        m_colorBlendAttachmentStates.resize(index + 1);
        m_colorBlendState.attachmentCount = static_cast<uint32_t>(m_colorBlendAttachmentStates.size());
        m_colorBlendState.pAttachments = m_colorBlendAttachmentStates.data();
    }
    m_colorBlendAttachmentStates[index].blendEnable = enabled;
    return *this;
}

PipelineBuilder& PipelineBuilder::setBlendFactors(uint32_t index, VkBlendFactor srcFactor, VkBlendFactor dstFactor) {
    if (index >= m_colorBlendAttachmentStates.size()) {
        m_colorBlendAttachmentStates.resize(index + 1);
        m_colorBlendState.attachmentCount = static_cast<uint32_t>(m_colorBlendAttachmentStates.size());
        m_colorBlendState.pAttachments = m_colorBlendAttachmentStates.data();
    }
    m_colorBlendAttachmentStates[index].srcColorBlendFactor = srcFactor;
    m_colorBlendAttachmentStates[index].srcAlphaBlendFactor = srcFactor;
    m_colorBlendAttachmentStates[index].dstColorBlendFactor = dstFactor;
    m_colorBlendAttachmentStates[index].dstAlphaBlendFactor = dstFactor;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthTest(VkBool32 enabled) {
    m_depthStencilState.depthTestEnable = enabled;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthTestOperation(VkCompareOp testOperation) {
    m_depthStencilState.depthCompareOp = testOperation;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthWrite(VkBool32 enabled) {
    m_depthStencilState.depthWriteEnable = enabled;
    return *this;
}

PipelineBuilder& PipelineBuilder::addDynamicState(const PipelineDynamicState dynamicState) {
    m_dynamicStateFlags |= dynamicState;
    return *this;
}

PipelineBuilder& PipelineBuilder::addDynamicStates(const PipelineDynamicStateFlags dynamicStates) {
    m_dynamicStateFlags |= dynamicStates;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDescriptorHeapMappings(
    const uint32_t shaderStageIdx, const std::span<const VkDescriptorSetAndBindingMappingEXT> mappings) {
    auto stage = std::make_unique<StageMappings>();
    stage->mappings.assign(mappings.begin(), mappings.end());
    stage->info.mappingCount = static_cast<uint32_t>(stage->mappings.size());
    stage->info.pMappings = stage->mappings.data();
    m_shaderStages.at(shaderStageIdx).pNext = &stage->info;
    m_stageMappings.push_back(std::move(stage));
    return *this;
}

std::unique_ptr<VulkanPipeline> PipelineBuilder::create(
    const VulkanDevice& device,
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor) {
    return createImpl(device, std::move(pipelineLayout), rasterizationPassDescriptor, false);
}

std::unique_ptr<VulkanPipeline> PipelineBuilder::createDescriptorHeap(
    const VulkanDevice& device, const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor) {
    return createImpl(device, nullptr, rasterizationPassDescriptor, true);
}

std::unique_ptr<VulkanPipeline> PipelineBuilder::createImpl(
    const VulkanDevice& device,
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor,
    const bool descriptorHeap) {
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    populatePipelineCreateInfo(pipelineInfo, pipelineLayout ? pipelineLayout->getHandle() : VK_NULL_HANDLE);

    const auto& colorFormats = rasterizationPassDescriptor.colorAttachmentFormats;
    const VkPipelineRenderingCreateInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .viewMask = rasterizationPassDescriptor.viewMask,
        .colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
        .pColorAttachmentFormats = !colorFormats.empty() ? colorFormats.data() : nullptr,
        .depthAttachmentFormat = rasterizationPassDescriptor.depthAttachmentFormat,
        .stencilAttachmentFormat = rasterizationPassDescriptor.stencilAttachmentFormat,
    };
    const VkPipelineCreateFlags2CreateInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
        .pNext = &renderingInfo,
        .flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
    };
    pipelineInfo.pNext = descriptorHeap ? static_cast<const void*>(&flagsInfo) : &renderingInfo;

    auto multisampleState = m_multisampleState;
    multisampleState.rasterizationSamples = rasterizationPassDescriptor.sampleCount;
    pipelineInfo.pMultisampleState = &multisampleState;

    auto colorBlendAttachmentStates = m_colorBlendAttachmentStates;
    colorBlendAttachmentStates.resize(colorFormats.size(), createDefaultColorBlendAttachmentState());
    auto colorBlendState = m_colorBlendState;
    colorBlendState.attachmentCount = static_cast<uint32_t>(colorBlendAttachmentStates.size());
    colorBlendState.pAttachments = colorBlendAttachmentStates.empty() ? nullptr : colorBlendAttachmentStates.data();
    pipelineInfo.pColorBlendState = &colorBlendState;

    const auto dynamicStates = toVkDynamicStates(m_dynamicStateFlags);
    const VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.empty() ? nullptr : dynamicStates.data(),
    };
    pipelineInfo.pDynamicState = &dynamicState;

    VkPipeline pipeline{VK_NULL_HANDLE};
    VK_FATAL(vkCreateGraphicsPipelines(
        device.getHandle(), device.getPipelineCacheHandle(), 1, &pipelineInfo, nullptr, &pipeline));
    return std::make_unique<VulkanPipeline>(
        device,
        pipeline,
        std::move(pipelineLayout),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        VulkanVertexLayout(m_vertexLayout),
        m_dynamicStateFlags);
}

void PipelineBuilder::populatePipelineCreateInfo(
    VkGraphicsPipelineCreateInfo& pipelineInfo, const VkPipelineLayout pipelineLayout) const {
    pipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
    pipelineInfo.pStages = m_shaderStages.data();
    pipelineInfo.pVertexInputState = &m_vertexInputState;
    pipelineInfo.pInputAssemblyState = &m_inputAssemblyState;
    pipelineInfo.pTessellationState = &m_tessellationState;
    pipelineInfo.pViewportState = &m_viewportState;
    pipelineInfo.pRasterizationState = &m_rasterizationState;
    pipelineInfo.pMultisampleState = &m_multisampleState;
    pipelineInfo.pColorBlendState = &m_colorBlendState;
    pipelineInfo.pDepthStencilState = &m_depthStencilState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
}

std::vector<VkDynamicState> toVkDynamicStates(const PipelineDynamicStateFlags flags) {
    std::vector<VkDynamicState> dynamicStates;
    for (const auto& mapping : kDynamicStateMappings) {
        if (flags.contains(mapping.state)) {
            dynamicStates.push_back(mapping.vkState);
        }
    }
    return dynamicStates;
}

PipelineDynamicState parsePipelineDynamicState(const std::string_view name) {
    const auto mapping = std::ranges::find(kDynamicStateMappings, name, [](const DynamicStateMapping& m) {
        return m.name;
    });
    return mapping == kDynamicStateMappings.end() ? PipelineDynamicState::None : mapping->state;
}

std::string_view toString(const PipelineDynamicState dynamicState) {
    const auto mapping = std::ranges::find(kDynamicStateMappings, dynamicState, [](const DynamicStateMapping& m) {
        return m.state;
    });
    return mapping == kDynamicStateMappings.end() ? "none" : mapping->name;
}

VkPipelineShaderStageCreateInfo createShaderStageInfo(
    VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint) {
    VkPipelineShaderStageCreateInfo shaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStageInfo.stage = shaderStage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = entryPoint;
    return shaderStageInfo;
}
} // namespace crisp
