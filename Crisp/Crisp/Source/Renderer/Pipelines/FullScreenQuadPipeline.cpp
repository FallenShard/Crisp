#include "FullScreenQuadPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    FullScreenQuadPipeline::FullScreenQuadPipeline(Renderer* renderer, VulkanRenderPass* renderPass, bool useGammaCorrection)
        : VulkanPipeline(renderer, 1, renderPass)
    {
        m_descriptorSetLayouts[DisplayedImage] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_SAMPLER,       1, VK_SHADER_STAGE_FRAGMENT_BIT },
            { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts,
        {
            { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int) }
        });

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER,       1 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 }
        }, 2);

        if (useGammaCorrection)
        {
            m_vertShader = renderer->getShaderModule("gamma-correct-vert");
            m_fragShader = renderer->getShaderModule("gamma-correct-frag");
        }
        else
        {
            m_vertShader = renderer->getShaderModule("fullscreen-quad-vert");
            m_fragShader = renderer->getShaderModule("fullscreen-quad-frag");
        }

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void FullScreenQuadPipeline::create(int width, int height)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages =
        {
            createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader),
            createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader)
        };

        std::vector<VkVertexInputBindingDescription> vertexInputBindings =
        {
            { 0, FormatSizeof<VK_FORMAT_R32G32_SFLOAT>::value, VK_VERTEX_INPUT_RATE_VERTEX }
        };
        
        std::vector<VkVertexInputAttributeDescription> attributes =
        {
            { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
        };

        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = static_cast<uint32_t>(vertexInputBindings.size());
        vertexInput.pVertexBindingDescriptions      = vertexInputBindings.data();
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions    = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = m_renderer->getDefaultViewport();

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
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable         = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        auto colorBlendState      = VulkanPipeline::createDefaultColorBlendState();
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments    = &colorBlendAttachment;

        auto depthStencilState = VulkanPipeline::createDefaultDepthStencilState();
        depthStencilState.depthTestEnable = false;
        depthStencilState.depthWriteEnable = false;

        std::vector<VkDynamicState> dynamicStates;
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages             = shaderStages.data();
        pipelineInfo.pVertexInputState   = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizationState;
        pipelineInfo.pMultisampleState   = &multisampleState;
        pipelineInfo.pColorBlendState    = &colorBlendState;
        pipelineInfo.pDepthStencilState  = &depthStencilState;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = m_pipelineLayout;
        pipelineInfo.renderPass          = m_renderPass->getHandle();
        pipelineInfo.subpass             = 0;
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex   = -1;

        vkCreateGraphicsPipelines(m_device->getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_handle);
    }
}