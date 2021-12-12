#include <Crisp/Vulkan/VulkanPipeline.hpp>

#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp
{
    VulkanPipeline::VulkanPipeline(const VulkanDevice& device, VkPipeline pipelineHandle, std::unique_ptr<VulkanPipelineLayout> pipelineLayout, PipelineDynamicStateFlags dynamicStateFlags)
        : VulkanResource(pipelineHandle, device.getResourceDeallocator())
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