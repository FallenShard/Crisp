#include "SkyboxPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    SkyboxPipeline::SkyboxPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass)
        : VulkanPipeline(renderer, 1, renderPass)
        , m_subpass(subpass)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts);

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
        }, 1);
        
        m_vertShader = m_renderer->getShaderModule("skybox-vert");
        m_fragShader = m_renderer->getShaderModule("skybox-frag");

        create(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height);
    }

    void SkyboxPipeline::create(int width, int height)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages =
        {
            createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader),
            createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader)
        };

        std::vector<VkVertexInputBindingDescription> vertexInputBindings =
        {
            { 0, FormatSizeof<VK_FORMAT_R32G32B32_SFLOAT>::value, VK_VERTEX_INPUT_RATE_VERTEX }
        };

        std::vector<VkVertexInputAttributeDescription> attributes =
        {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
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
        rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;

        auto multisampleState     = VulkanPipeline::createDefaultMultisampleState();
        auto colorBlendAttachment = VulkanPipeline::createDefaultColorBlendAttachmentState();
        auto colorBlendState      = VulkanPipeline::createDefaultColorBlendState();
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments = &colorBlendAttachment;
        auto depthStencilState  = VulkanPipeline::createDefaultDepthStencilState();

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
        pipelineInfo.pDynamicState       = nullptr;
        pipelineInfo.layout              = m_pipelineLayout;
        pipelineInfo.renderPass          = m_renderPass->getHandle();
        pipelineInfo.subpass             = m_subpass;
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex   = -1;

        vkCreateGraphicsPipelines(m_renderer->getDevice()->getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_handle);
    }
}