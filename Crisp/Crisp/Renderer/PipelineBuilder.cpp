#include <Crisp/Renderer/PipelineBuilder.hpp>

#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {

enum class VertexAttributeLayout {
    Interleaved,
    Concatenated
};

std::vector<VkVertexInputAttributeDescription> generateVertexInputAttributes(
    uint32_t locationOffset,
    uint32_t binding,
    const std::vector<VkFormat>& formats,
    const VertexAttributeLayout layout) {
    std::vector<VkVertexInputAttributeDescription> vertexAttribs(formats.size());

    uint32_t offset = 0;
    for (uint32_t i = 0; i < vertexAttribs.size(); ++i) {
        vertexAttribs[i].location = locationOffset + i;
        vertexAttribs[i].binding = binding;
        vertexAttribs[i].format = formats[i];
        vertexAttribs[i].offset = layout == VertexAttributeLayout::Interleaved ? offset : 0;
        offset += getSizeOf(formats[i]);
    }

    return vertexAttribs;
}

PipelineDynamicState getPipelineDynamicState(VkDynamicState dynamicState) {
    switch (dynamicState) {
    case VK_DYNAMIC_STATE_VIEWPORT:
        return PipelineDynamicState::Viewport;
    case VK_DYNAMIC_STATE_SCISSOR:
        return PipelineDynamicState::Scissor;
    default: {
        spdlog::critical("Invalid vulkan dynamic state received!");
    }
    }

    return static_cast<crisp::PipelineDynamicState>(0);
}
} // namespace

PipelineBuilder::PipelineBuilder()
    : m_vertexInputState({VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO})
    , m_inputAssemblyState({VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO})
    , m_viewportState({VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO})
    , m_tessellationState({VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO})
    , m_rasterizationState(createDefaultRasterizationState())
    , m_multisampleState(createDefaultMultisampleState())
    , m_colorBlendState(createDefaultColorBlendState())
    , m_colorBlendAttachmentStates({createDefaultColorBlendAttachmentState()})
    , m_depthStencilState(createDefaultDepthStencilState())
    , m_dynamicState({VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO})
    , m_pipelineStateFlags(PipelineState::Default) {
    m_colorBlendState.attachmentCount = static_cast<uint32_t>(m_colorBlendAttachmentStates.size());
    m_colorBlendState.pAttachments = m_colorBlendAttachmentStates.data();

    m_inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_inputAssemblyState.primitiveRestartEnable = VK_FALSE;
}

PipelineBuilder& PipelineBuilder::addShaderStage(VkPipelineShaderStageCreateInfo&& shaderStage) {
    m_shaderStages.emplace_back(shaderStage);
    return *this;
}

PipelineBuilder& PipelineBuilder::setShaderStages(std::initializer_list<VkPipelineShaderStageCreateInfo> shaderStages) {
    m_shaderStages = shaderStages;
    return *this;
}

PipelineBuilder& PipelineBuilder::setShaderStages(std::vector<VkPipelineShaderStageCreateInfo>&& shaderStages) {
    m_shaderStages = shaderStages;
    return *this;
}

PipelineBuilder& PipelineBuilder::addVertexInputBinding(
    uint32_t binding, VkVertexInputRate inputRate, const std::vector<VkFormat>& formats) {
    uint32_t aggregateSize = 0;
    for (const auto format : formats) {
        aggregateSize += getSizeOf(format);
    }

    m_vertexLayout.bindings.push_back({binding, aggregateSize, inputRate});
    m_vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexLayout.bindings.size());
    m_vertexInputState.pVertexBindingDescriptions = m_vertexLayout.bindings.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::addVertexAttributes(uint32_t binding, const std::vector<VkFormat>& formats) {
    const uint32_t locOffset = static_cast<uint32_t>(m_vertexLayout.attributes.size());
    auto attribs = generateVertexInputAttributes(locOffset, binding, formats, VertexAttributeLayout::Interleaved);
    m_vertexLayout.attributes.insert(m_vertexLayout.attributes.end(), attribs.begin(), attribs.end());
    m_vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexLayout.attributes.size());
    m_vertexInputState.pVertexAttributeDescriptions = m_vertexLayout.attributes.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::setFullScreenVertexLayout() {
    m_vertexLayout.bindings.clear();
    addVertexInputBinding(0, VK_VERTEX_INPUT_RATE_VERTEX, {VK_FORMAT_R32G32_SFLOAT});

    m_vertexLayout.attributes.clear();
    addVertexAttributes(0, {VK_FORMAT_R32G32_SFLOAT});
    return *this;
}

PipelineBuilder& PipelineBuilder::setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable) {
    m_inputAssemblyState.topology = topology;
    m_inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;
    return *this;
}

PipelineBuilder& PipelineBuilder::setTessellationControlPoints(uint32_t numControlPoints) {
    m_pipelineStateFlags |= PipelineState::Tessellation;
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

PipelineBuilder& PipelineBuilder::setSampleCount(VkSampleCountFlagBits sampleCount) {
    m_multisampleState.rasterizationSamples = sampleCount;
    return *this;
}

PipelineBuilder& PipelineBuilder::setAlphaToCoverage(VkBool32 alphaToCoverageEnabled) {
    m_multisampleState.alphaToCoverageEnable = alphaToCoverageEnabled;
    return *this;
}

PipelineBuilder& PipelineBuilder::setViewport(VkViewport&& viewport) {
    m_viewports = {viewport};
    m_viewportState.viewportCount = static_cast<uint32_t>(m_viewports.size());
    m_viewportState.pViewports = m_viewports.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::setScissor(VkRect2D&& scissor) {
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

PipelineBuilder& PipelineBuilder::enableState(PipelineState pipelineState) {
    m_pipelineStateFlags |= pipelineState;
    return *this;
}

PipelineBuilder& PipelineBuilder::disableState(PipelineState pipelineState) {
    m_pipelineStateFlags.disable(pipelineState);
    return *this;
}

PipelineBuilder& PipelineBuilder::addDynamicState(VkDynamicState dynamicState) {
    m_pipelineStateFlags |= PipelineState::Dynamic;
    m_dynamicStates.push_back(dynamicState);
    m_dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
    m_dynamicState.pDynamicStates = m_dynamicStates.data();
    return *this;
}

std::unique_ptr<VulkanPipeline> PipelineBuilder::create(
    const VulkanDevice& device,
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
    VkRenderPass renderPass,
    uint32_t subpassIndex) {
    VkGraphicsPipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
    pipelineInfo.pStages = m_shaderStages.data();
    pipelineInfo.pVertexInputState = m_pipelineStateFlags & PipelineState::VertexInput ? &m_vertexInputState : nullptr;
    pipelineInfo.pInputAssemblyState =
        m_pipelineStateFlags & PipelineState::InputAssembly ? &m_inputAssemblyState : nullptr;
    pipelineInfo.pTessellationState =
        m_pipelineStateFlags & PipelineState::Tessellation ? &m_tessellationState : nullptr;
    pipelineInfo.pViewportState = m_pipelineStateFlags & PipelineState::Viewport ? &m_viewportState : nullptr;
    pipelineInfo.pRasterizationState =
        m_pipelineStateFlags & PipelineState::Rasterization ? &m_rasterizationState : nullptr;
    pipelineInfo.pMultisampleState = m_pipelineStateFlags & PipelineState::Multisample ? &m_multisampleState : nullptr;
    pipelineInfo.pColorBlendState = m_pipelineStateFlags & PipelineState::ColorBlend ? &m_colorBlendState : nullptr;
    pipelineInfo.pDepthStencilState =
        m_pipelineStateFlags & PipelineState::DepthStencil ? &m_depthStencilState : nullptr;
    pipelineInfo.pDynamicState = m_pipelineStateFlags & PipelineState::Dynamic ? &m_dynamicState : nullptr;
    pipelineInfo.layout = pipelineLayout->getHandle();
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = subpassIndex;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(device.getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    return std::make_unique<VulkanPipeline>(
        device,
        pipeline,
        std::move(pipelineLayout),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        VertexLayout(m_vertexLayout),
        createDynamicStateFlags());
}

PipelineDynamicStateFlags PipelineBuilder::createDynamicStateFlags() const {
    PipelineDynamicStateFlags dynamicStateFlags;
    for (auto dynState : m_dynamicStates) {
        dynamicStateFlags |= getPipelineDynamicState(dynState);
    }

    return dynamicStateFlags;
}

VkPipelineRasterizationStateCreateInfo PipelineBuilder::createDefaultRasterizationState() {
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

VkPipelineMultisampleStateCreateInfo PipelineBuilder::createDefaultMultisampleState() {
    VkPipelineMultisampleStateCreateInfo multisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.sampleShadingEnable = VK_FALSE;
    multisampleState.minSampleShading = 1.0f;
    multisampleState.pSampleMask = nullptr;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.alphaToOneEnable = VK_FALSE;
    return multisampleState;
}

VkPipelineColorBlendAttachmentState PipelineBuilder::createDefaultColorBlendAttachmentState() {
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

VkPipelineColorBlendStateCreateInfo PipelineBuilder::createDefaultColorBlendState() {
    VkPipelineColorBlendStateCreateInfo colorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    colorBlendState.blendConstants[0] = 0.0f;
    colorBlendState.blendConstants[1] = 0.0f;
    colorBlendState.blendConstants[2] = 0.0f;
    colorBlendState.blendConstants[3] = 0.0f;
    return colorBlendState;
}

VkPipelineDepthStencilStateCreateInfo PipelineBuilder::createDefaultDepthStencilState() {
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

VkPipelineShaderStageCreateInfo createShaderStageInfo(
    VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint) {
    VkPipelineShaderStageCreateInfo shaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStageInfo.stage = shaderStage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = entryPoint;
    return shaderStageInfo;
}
} // namespace crisp
