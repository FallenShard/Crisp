#include "VulkanPipeline.hpp"
#include "vulkan/VulkanRenderer.hpp"

namespace crisp
{
    VulkanPipeline::VulkanPipeline(VulkanRenderer* renderer, uint32_t layoutCount, VulkanRenderPass* renderPass)
        : m_device(renderer->getDevice().getHandle())
        , m_renderer(renderer)
        , m_renderPass(renderPass)
        , m_pipelineLayout(nullptr)
        , m_pipeline(nullptr)
        , m_descriptorPools(layoutCount, nullptr)
        , m_descriptorSetLayouts(layoutCount, nullptr)
    {
    }

    VulkanPipeline::~VulkanPipeline()
    {
        for (auto& pool : m_descriptorPools)
            if (pool != nullptr)
                vkDestroyDescriptorPool(m_device, pool, nullptr);

        for (auto& layout : m_descriptorSetLayouts)
            if (layout != nullptr)
                vkDestroyDescriptorSetLayout(m_device, layout, nullptr);

        if (m_pipelineLayout != nullptr)
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

        if (m_pipeline != nullptr)
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }

    void VulkanPipeline::resize(int width, int height)
    {
        if (m_pipeline != nullptr)
            vkDestroyPipeline(m_device, m_pipeline, nullptr);

        create(width, height);
    }

    VkDescriptorSet VulkanPipeline::allocateDescriptorSet(uint32_t setId) const
    {
        VkDescriptorSetAllocateInfo descSetInfo = {};
        descSetInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descSetInfo.descriptorPool     = m_descriptorPools.at(setId);
        descSetInfo.descriptorSetCount = 1;
        descSetInfo.pSetLayouts        = &m_descriptorSetLayouts.at(setId);

        VkDescriptorSet descSet;
        vkAllocateDescriptorSets(m_renderer->getDevice().getHandle(), &descSetInfo, &descSet);
        return descSet;
    }

    VkPipelineRasterizationStateCreateInfo VulkanPipeline::createDefaultRasterizationState()
    {
        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.depthClampEnable        = VK_FALSE;
        rasterizationState.rasterizerDiscardEnable = VK_FALSE;
        rasterizationState.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth               = 1.0f;
        rasterizationState.cullMode                = VK_CULL_MODE_NONE;
        rasterizationState.frontFace               = VK_FRONT_FACE_CLOCKWISE;
        rasterizationState.depthBiasEnable         = VK_FALSE;
        rasterizationState.depthBiasClamp          = 0.0f;
        rasterizationState.depthBiasConstantFactor = 0.0f;
        rasterizationState.depthBiasSlopeFactor    = 0.0f;
        return rasterizationState;
    }

    VkPipelineMultisampleStateCreateInfo VulkanPipeline::createDefaultMultisampleState()
    {
        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.sampleShadingEnable   = VK_FALSE;
        multisampleState.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
        multisampleState.minSampleShading      = 1.f;
        multisampleState.pSampleMask           = nullptr;
        multisampleState.alphaToCoverageEnable = VK_FALSE;
        multisampleState.alphaToOneEnable      = VK_FALSE;
        return multisampleState;
    }

    VkPipelineColorBlendAttachmentState VulkanPipeline::createDefaultColorBlendAttachmentState()
    {
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable         = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        return colorBlendAttachment;
    }

    VkPipelineColorBlendStateCreateInfo VulkanPipeline::createDefaultColorBlendState()
    {
        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.logicOpEnable     = VK_FALSE;
        colorBlendState.logicOp           = VK_LOGIC_OP_COPY;
        colorBlendState.blendConstants[0] = 0.0f;
        colorBlendState.blendConstants[1] = 0.0f;
        colorBlendState.blendConstants[2] = 0.0f;
        colorBlendState.blendConstants[3] = 0.0f;
        return colorBlendState;
    }

    VkPipelineDepthStencilStateCreateInfo VulkanPipeline::createDefaultDepthStencilState()
    {
        VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
        depthStencilState.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilState.depthTestEnable       = VK_TRUE;
        depthStencilState.depthWriteEnable      = VK_TRUE;
        depthStencilState.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencilState.depthBoundsTestEnable = VK_FALSE;
        depthStencilState.minDepthBounds        = 0.0f;
        depthStencilState.maxDepthBounds        = 1.0f;
        depthStencilState.stencilTestEnable     = VK_FALSE;
        depthStencilState.front                 = {};
        depthStencilState.back                  = {};
        return depthStencilState;
    }
}