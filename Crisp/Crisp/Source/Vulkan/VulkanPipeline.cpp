#include "Vulkan/VulkanPipeline.hpp"

#include <algorithm>

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    VulkanPipeline::VulkanPipeline(VulkanDevice* device, VkPipeline pipelineHandle, std::unique_ptr<VulkanPipelineLayout> pipelineLayout, PipelineDynamicStateFlags dynamicStateFlags)
        : VulkanResource(device, pipelineHandle)
        , m_pipelineLayout(std::move(pipelineLayout))
        , m_dynamicStateFlags(dynamicStateFlags)
    {
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (m_deferDestruction)
            m_device->deferDestruction(m_handle, vkDestroyPipeline);
        else
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

    VulkanDescriptorSet VulkanPipeline::allocateDescriptorSet(uint32_t setId) const
    {
        return VulkanDescriptorSet(setId, m_pipelineLayout.get());
    }
}