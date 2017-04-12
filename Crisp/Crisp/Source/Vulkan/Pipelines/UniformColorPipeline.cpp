#include "UniformColorPipeline.hpp"

#include "Vulkan/VulkanRenderer.hpp"

namespace crisp
{
    UniformColorPipeline::UniformColorPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass)
        : VulkanPipeline(renderer, 2, renderPass)
    {
        VkDescriptorSetLayoutBinding transformBinding = {};
        transformBinding.binding            = 0;
        transformBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        transformBinding.descriptorCount    = 1;
        transformBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
        transformBinding.pImmutableSamplers = nullptr;

        std::vector<VkDescriptorSetLayoutBinding> firstSetBindings =
        {
            transformBinding
        };
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(firstSetBindings.size());
        layoutInfo.pBindings    = firstSetBindings.data();
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[0]);

        VkDescriptorSetLayoutBinding colorBinding = {};
        colorBinding.binding            = 0;
        colorBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        colorBinding.descriptorCount    = 1;
        colorBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        colorBinding.pImmutableSamplers = nullptr;

        std::vector<VkDescriptorSetLayoutBinding> secondSetBindings =
        {
            colorBinding
        };
        layoutInfo = {};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(secondSetBindings.size());
        layoutInfo.pBindings    = secondSetBindings.data();
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[1]);

        // Push constants
        std::vector<VkPushConstantRange> pushConstants(1, VkPushConstantRange{});
        pushConstants[0].offset     = 0;
        pushConstants[0].size       = sizeof(int);
        pushConstants[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(m_descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts            = m_descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
        pipelineLayoutInfo.pPushConstantRanges    = pushConstants.data();
        vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);

        // Descriptor Pool
        std::array<VkDescriptorPoolSize, 2> poolSizes = {};
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes    = &poolSizes[0];
        poolInfo.maxSets       = 2;
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool);

        m_vertShader = renderer->getShaderModule("unif-col-vert");
        m_fragShader = renderer->getShaderModule("unif-col-frag");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void UniformColorPipeline::create(int width, int height)
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
        bindingDescription.stride    = sizeof(glm::vec3);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);
        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
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
        colorBlendState.pAttachments = &colorBlendAttachment;

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

        vkCreateGraphicsPipelines(m_renderer->getDevice()->getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
    }
}