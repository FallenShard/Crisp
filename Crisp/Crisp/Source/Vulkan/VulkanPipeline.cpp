#include "Vulkan/VulkanPipeline.hpp"

#include <algorithm>

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanRenderPass.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    VulkanPipeline::VulkanPipeline(Renderer* renderer, uint32_t layoutCount, VulkanRenderPass* renderPass, bool isWindowDependent)
        : VulkanResource(renderer->getDevice())
        , m_renderer(renderer)
        , m_renderPass(renderPass)
        , m_pipelineLayout(VK_NULL_HANDLE)
        , m_descriptorPool(VK_NULL_HANDLE)
        , m_descriptorSetLayouts(layoutCount)
        , m_isWindowDependent(isWindowDependent)
    {
        if (m_isWindowDependent)
            m_renderer->registerPipeline(this);
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (m_isWindowDependent)
            m_renderer->unregisterPipeline(this);

        if (m_descriptorPool != nullptr)
            vkDestroyDescriptorPool(m_device->getHandle(), m_descriptorPool, nullptr);

        if (m_pipelineLayout != nullptr)
            vkDestroyPipelineLayout(m_device->getHandle(), m_pipelineLayout, nullptr);

        if (m_handle != nullptr)
            vkDestroyPipeline(m_device->getHandle(), m_handle, nullptr);
    }

    void VulkanPipeline::bind(VkCommandBuffer cmdBuffer) const
    {
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_handle);
    }

    void VulkanPipeline::bind(VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint) const
    {
        vkCmdBindPipeline(cmdBuffer, bindPoint, m_handle);
    }

    void VulkanPipeline::resize(int width, int height)
    {
        if (m_isWindowDependent && m_handle != nullptr)
            vkDestroyPipeline(m_device->getHandle(), m_handle, nullptr);

        create(width, height);
    }

    VulkanDescriptorSet VulkanPipeline::allocateDescriptorSet(uint32_t setId) const
    {
        return VulkanDescriptorSet(this, setId);
    }

    VulkanDescriptorSetLayout* VulkanPipeline::getDescriptorSetLayout(uint32_t index) const
    {
        return m_descriptorSetLayouts[index].get();
    }

    VkPipelineRasterizationStateCreateInfo VulkanPipeline::createDefaultRasterizationState()
    {
        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.depthClampEnable        = VK_FALSE;
        rasterizationState.rasterizerDiscardEnable = VK_FALSE;
        rasterizationState.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth               = 1.0f;
        rasterizationState.cullMode                = VK_CULL_MODE_BACK_BIT;
        rasterizationState.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

    std::unique_ptr<VulkanDescriptorSetLayout> VulkanPipeline::createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayoutCreateFlags flags)
    {
        return std::make_unique<VulkanDescriptorSetLayout>(m_device, bindings, flags);
    }

    VkDescriptorPool VulkanPipeline::createDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxAllocatedSets)
    {
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        poolInfo.maxSets       = maxAllocatedSets;

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(m_device->getHandle(), &poolInfo, nullptr, &descriptorPool);
        return descriptorPool;
    }

    VkPipelineLayout VulkanPipeline::createPipelineLayout(const std::vector<std::unique_ptr<VulkanDescriptorSetLayout>>& setLayouts, const std::vector<VkPushConstantRange>& pushConstants)
    {
        std::vector<VkDescriptorSetLayout> rawSetLayouts;
        std::transform(setLayouts.begin(), setLayouts.end(), std::back_inserter(rawSetLayouts), [](const auto& item) { return item->getHandle(); });

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(rawSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts            = rawSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
        pipelineLayoutInfo.pPushConstantRanges    = pushConstants.data();

        VkPipelineLayout layout;
        vkCreatePipelineLayout(m_device->getHandle(), &pipelineLayoutInfo, nullptr, &layout);
        return layout;
    }

    VkPipelineShaderStageCreateInfo VulkanPipeline::createShaderStageInfo(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint)
    {
        VkPipelineShaderStageCreateInfo shaderStageInfo = {};
        shaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage  = shaderStage;
        shaderStageInfo.module = shaderModule;
        shaderStageInfo.pName  = entryPoint;

        return shaderStageInfo;
    }
}