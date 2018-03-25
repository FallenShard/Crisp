#include "Renderer/PipelineBuilder.hpp"

#include "vulkan/VulkanPipeline.hpp"

namespace crisp
{
    PipelineBuilder::PipelineBuilder()
        : m_vertexInputState({ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO })
        , m_inputAssemblyState({ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO })
        , m_viewportState({ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO })
        , m_tessellationState({ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO })
        , m_rasterizationState(createDefaultRasterizationState())
        , m_multisampleState(createDefaultMultisampleState())
        , m_colorBlendState(createDefaultColorBlendState())
        , m_colorBlendAttachmentStates({ createDefaultColorBlendAttachmentState() })
        , m_depthStencilState(createDefaultDepthStencilState())
        , m_dynamicState({ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO })
        , m_pipelineStateFlags(PipelineState::Default)
    {
        m_colorBlendState.attachmentCount = static_cast<uint32_t>(m_colorBlendAttachmentStates.size());
        m_colorBlendState.pAttachments = m_colorBlendAttachmentStates.data();

        m_inputAssemblyState.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        m_inputAssemblyState.primitiveRestartEnable = VK_FALSE;
    }

    PipelineBuilder& PipelineBuilder::addShaderStage(VkPipelineShaderStageCreateInfo&& shaderStage)
    {
        m_shaderStages.emplace_back(shaderStage);
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setShaderStages(std::initializer_list<VkPipelineShaderStageCreateInfo> shaderStages)
    {
        m_shaderStages = shaderStages;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)
    {
        m_inputAssemblyState.topology               = topology;
        m_inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setViewport(VkViewport&& viewport)
    {
        m_viewports = { viewport };
        m_viewportState.viewportCount = static_cast<uint32_t>(m_viewports.size());
        m_viewportState.pViewports    = m_viewports.data();
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setScissor(VkRect2D&& scissor)
    {
        m_scissors = { scissor };
        m_viewportState.scissorCount = static_cast<uint32_t>(m_scissors.size());
        m_viewportState.pScissors    = m_scissors.data();
        return *this;
    }

    void PipelineBuilder::enableState(PipelineState pipelineState)
    {
        m_pipelineStateFlags |= pipelineState;
    }

    void PipelineBuilder::disableState(PipelineState pipelineState)
    {
        m_pipelineStateFlags.disable(pipelineState);
    }

    VkPipeline PipelineBuilder::create(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, uint32_t subpassIndex)
    {
        VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.stageCount          = static_cast<uint32_t>(m_shaderStages.size());
        pipelineInfo.pStages             = m_shaderStages.data();
        pipelineInfo.pVertexInputState   = m_pipelineStateFlags & PipelineState::VertexInput   ? &m_vertexInputState   : nullptr;
        pipelineInfo.pInputAssemblyState = m_pipelineStateFlags & PipelineState::InputAssembly ? &m_inputAssemblyState : nullptr;
        pipelineInfo.pTessellationState  = m_pipelineStateFlags & PipelineState::Tessellation  ? &m_tessellationState  : nullptr;
        pipelineInfo.pViewportState      = m_pipelineStateFlags & PipelineState::Viewport      ? &m_viewportState      : nullptr;
        pipelineInfo.pRasterizationState = m_pipelineStateFlags & PipelineState::Rasterization ? &m_rasterizationState : nullptr;
        pipelineInfo.pMultisampleState   = m_pipelineStateFlags & PipelineState::Multisample   ? &m_multisampleState   : nullptr;
        pipelineInfo.pColorBlendState    = m_pipelineStateFlags & PipelineState::ColorBlend    ? &m_colorBlendState    : nullptr;
        pipelineInfo.pDepthStencilState  = m_pipelineStateFlags & PipelineState::DepthStencil  ? &m_depthStencilState  : nullptr;
        pipelineInfo.pDynamicState       = m_pipelineStateFlags & PipelineState::Dynamic       ? &m_dynamicState       : nullptr;
        pipelineInfo.layout              = pipelineLayout;
        pipelineInfo.renderPass          = renderPass;
        pipelineInfo.subpass             = subpassIndex;
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex   = -1;

        VkPipeline pipeline;
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        return pipeline;
    }

    VkPipelineRasterizationStateCreateInfo PipelineBuilder::createDefaultRasterizationState()
    {
        VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationState.depthClampEnable        = VK_FALSE;
        rasterizationState.rasterizerDiscardEnable = VK_FALSE;
        rasterizationState.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterizationState.cullMode                = VK_CULL_MODE_BACK_BIT;
        rasterizationState.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationState.depthBiasEnable         = VK_FALSE;
        rasterizationState.depthBiasClamp          = 0.0f;
        rasterizationState.depthBiasConstantFactor = 0.0f;
        rasterizationState.depthBiasSlopeFactor    = 0.0f;
        rasterizationState.lineWidth               = 1.0f;
        return rasterizationState;
    }

    VkPipelineMultisampleStateCreateInfo PipelineBuilder::createDefaultMultisampleState()
    {
        VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleState.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
        multisampleState.sampleShadingEnable   = VK_FALSE;
        multisampleState.minSampleShading      = 1.0f;
        multisampleState.pSampleMask           = nullptr;
        multisampleState.alphaToCoverageEnable = VK_FALSE;
        multisampleState.alphaToOneEnable      = VK_FALSE;
        return multisampleState;
    }

    VkPipelineColorBlendAttachmentState PipelineBuilder::createDefaultColorBlendAttachmentState()
    {
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.blendEnable         = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        return colorBlendAttachment;
    }

    VkPipelineColorBlendStateCreateInfo PipelineBuilder::createDefaultColorBlendState()
    {
        VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendState.logicOpEnable     = VK_FALSE;
        colorBlendState.logicOp           = VK_LOGIC_OP_COPY;
        colorBlendState.blendConstants[0] = 0.0f;
        colorBlendState.blendConstants[1] = 0.0f;
        colorBlendState.blendConstants[2] = 0.0f;
        colorBlendState.blendConstants[3] = 0.0f;
        return colorBlendState;
    }

    VkPipelineDepthStencilStateCreateInfo PipelineBuilder::createDefaultDepthStencilState()
    {
        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencilState.depthTestEnable       = VK_TRUE;
        depthStencilState.depthWriteEnable      = VK_TRUE;
        depthStencilState.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencilState.depthBoundsTestEnable = VK_FALSE;
        depthStencilState.stencilTestEnable     = VK_FALSE;
        depthStencilState.front                 = {};
        depthStencilState.back                  = {};
        depthStencilState.minDepthBounds        = 0.0f;
        depthStencilState.maxDepthBounds        = 1.0f;
        return depthStencilState;
    }
}