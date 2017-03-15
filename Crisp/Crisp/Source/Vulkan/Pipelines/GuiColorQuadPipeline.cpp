#include "GuiColorQuadPipeline.hpp"

#include "Vulkan/VulkanRenderer.hpp"

namespace crisp
{
    GuiColorQuadPipeline::GuiColorQuadPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass)
        : VulkanPipeline(renderer, DescSets::Count, renderPass)
    {
        VkDescriptorSetLayoutBinding transformBinding = {};
        transformBinding.binding            = 0;
        transformBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        transformBinding.descriptorCount    = 1;
        transformBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
        transformBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding colorBinding = {};
        colorBinding.binding            = 1;
        colorBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        colorBinding.descriptorCount    = 1;
        colorBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        colorBinding.pImmutableSamplers = nullptr;

        std::vector<VkDescriptorSetLayoutBinding> setBindings =
        {
            transformBinding,
            colorBinding
        };
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(setBindings.size());
        layoutInfo.pBindings    = setBindings.data();
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[DescSets::TransformAndColor]);

        // Push constants
        std::vector<VkPushConstantRange> pushConstants(2, VkPushConstantRange{});
        pushConstants[0].offset     = 0;
        pushConstants[0].size       = sizeof(int);
        pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstants[1].offset     = 4;
        pushConstants[1].size       = sizeof(int);
        pushConstants[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(m_descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts            = m_descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
        pipelineLayoutInfo.pPushConstantRanges    = pushConstants.data();
        vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);

        // Descriptor Pool
        std::vector<VkDescriptorPoolSize> poolSizes =
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3 }
        };

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        poolInfo.maxSets       = 3;
        vkCreateDescriptorPool(m_renderer->getDevice().getHandle(), &poolInfo, nullptr, &m_descriptorPool);

        m_vertShader = renderer->getShaderModule("gui-quad-unif-col-vert");
        m_fragShader = renderer->getShaderModule("gui-quad-unif-col-frag");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void GuiColorQuadPipeline::create(int width, int height)
    {
        // Shader stages
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = m_vertShader;
        vertShaderStageInfo.pName  = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = m_fragShader;
        fragShaderStageInfo.pName  = "main";

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages =
        {
            vertShaderStageInfo,
            fragShaderStageInfo
        };

        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof(glm::vec2);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);
        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset   = 0;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount   = 1;
        vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x        = 0.f;
        viewport.y        = 0.f;
        viewport.width    = static_cast<float>(m_renderer->getSwapChainExtent().width);
        viewport.height   = static_cast<float>(m_renderer->getSwapChainExtent().height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = m_renderer->getSwapChainExtent();

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        auto rasterizationState   = VulkanPipeline::createDefaultRasterizationState();
        auto multisampleState     = VulkanPipeline::createDefaultMultisampleState();
        auto colorBlendAttachment = VulkanPipeline::createDefaultColorBlendAttachmentState();
        auto colorBlendState      = VulkanPipeline::createDefaultColorBlendState();
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments    = &colorBlendAttachment;

        auto depthStencilState = VulkanPipeline::createDefaultDepthStencilState();

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages             = shaderStages.data();
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizationState;
        pipelineInfo.pMultisampleState   = &multisampleState;
        pipelineInfo.pColorBlendState    = &colorBlendState;
        pipelineInfo.pDepthStencilState  = &depthStencilState;
        pipelineInfo.pDynamicState       = nullptr;
        pipelineInfo.layout              = m_pipelineLayout;
        pipelineInfo.renderPass          = m_renderPass->getHandle();
        pipelineInfo.subpass             = 0;
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex   = -1;

        vkCreateGraphicsPipelines(m_renderer->getDevice().getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
    }
}