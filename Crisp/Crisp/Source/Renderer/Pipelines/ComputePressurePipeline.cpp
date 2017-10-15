#include "ComputePressurePipeline.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    ComputePressurePipeline::ComputePressurePipeline(VulkanRenderer* renderer)
        : VulkanPipeline(renderer, 1, nullptr)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
            { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
            { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
            { 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT }
        });

        std::vector<VkPushConstantRange> pushConstants =
        {
            { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float) }
        };

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts, pushConstants);

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
        }, 1);

        m_shader = renderer->getShaderModule("compute-pressure-comp");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void ComputePressurePipeline::create(int width, int height)
    {
        VkPipelineShaderStageCreateInfo shaderInfo = createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, m_shader);

        VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        pipelineInfo.stage              = shaderInfo;
        pipelineInfo.layout             = m_pipelineLayout;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex  = -1;
        vkCreateComputePipelines(m_device->getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_handle);
    }
}