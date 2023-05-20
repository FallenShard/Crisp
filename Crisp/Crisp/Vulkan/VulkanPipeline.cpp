#include <Crisp/Vulkan/VulkanPipeline.hpp>

namespace crisp
{
VulkanPipeline::VulkanPipeline(
    const VulkanDevice& device,
    const VkPipeline pipelineHandle,
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
    const VkPipelineBindPoint bindPoint,
    VertexLayout&& vertexLayout,
    const PipelineDynamicStateFlags dynamicStateFlags)
    : VulkanResource(pipelineHandle, device.getResourceDeallocator())
    , m_pipelineLayout(std::move(pipelineLayout))
    , m_dynamicStateFlags(dynamicStateFlags)
    , m_bindPoint(bindPoint)
    , m_vertexLayout(std::move(vertexLayout))
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
} // namespace crisp