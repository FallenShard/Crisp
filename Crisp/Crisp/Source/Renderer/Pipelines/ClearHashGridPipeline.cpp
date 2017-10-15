#include "ClearHashGridPipeline.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    ClearHashGridPipeline::ClearHashGridPipeline(VulkanRenderer* renderer)
        : VulkanPipeline(renderer, 1, nullptr)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts);

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
        }, 1);

        m_shader = renderer->getShaderModule("clear-hash-grid-comp");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void ClearHashGridPipeline::create(int width, int height)
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