#include "Vulkan/VulkanPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"

namespace crisp
{
    namespace
    {
    }

    VulkanPipeline::VulkanPipeline(VulkanDevice* device, VkPipeline pipelineHandle, std::unique_ptr<VulkanPipelineLayout> pipelineLayout, PipelineDynamicStateFlags dynamicStateFlags)
        : VulkanResource(device, pipelineHandle)
        , m_pipelineLayout(std::move(pipelineLayout))
        , m_dynamicStateFlags(dynamicStateFlags)
        , m_subpassIndex(0)
        , m_bindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
    }

    void VulkanPipeline::bind(VkCommandBuffer cmdBuffer) const
    {
        vkCmdBindPipeline(cmdBuffer, m_bindPoint, m_handle);
    }

    VulkanDescriptorSet VulkanPipeline::allocateDescriptorSet(uint32_t setId) const
    {
        return VulkanDescriptorSet(setId, m_pipelineLayout.get());
    }

    void VulkanPipeline::swapAll(VulkanPipeline& other)
    {
        swap(other);
        m_pipelineLayout->swap(*other.m_pipelineLayout);
    }
}