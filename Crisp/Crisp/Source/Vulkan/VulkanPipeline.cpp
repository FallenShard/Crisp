#include "Vulkan/VulkanPipeline.hpp"

#include <algorithm>

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    VulkanPipeline::VulkanPipeline(VulkanDevice* device, VkPipeline pipelineHandle, std::unique_ptr<VulkanPipelineLayout> pipelineLayout)
        : VulkanResource(device, pipelineHandle)
        , m_pipelineLayout(std::move(pipelineLayout))
    {
    }

    VulkanPipeline::~VulkanPipeline()
    {
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
        return VulkanDescriptorSet(m_pipelineLayout->allocateSet(setId), setId, m_pipelineLayout.get());
    }
}