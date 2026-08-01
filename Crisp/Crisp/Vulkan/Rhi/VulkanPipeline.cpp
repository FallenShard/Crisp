#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

#include <Crisp/Core/Format.hpp>

namespace crisp {
VulkanPipeline::VulkanPipeline(
    const VulkanDevice& device,
    const VkPipeline pipelineHandle,
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
    const VkPipelineBindPoint bindPoint,
    VulkanVertexLayout&& vertexLayout,
    const PipelineDynamicStateFlags dynamicStateFlags)
    : VulkanResource(pipelineHandle, device.getResourceDeallocator())
    , m_pipelineLayout(std::move(pipelineLayout))
    , m_dynamicStateFlags(dynamicStateFlags)
    , m_vertexLayout(std::move(vertexLayout))
    , m_bindPoint(bindPoint) {}

void VulkanPipeline::bind(VkCommandBuffer cmdBuffer) const {
    vkCmdBindPipeline(cmdBuffer, m_bindPoint, m_handle);
}

void VulkanPipeline::setDebugName(const VulkanDevice& device, const std::string_view name) const {
    device.setObjectName(*this, fmt::format("{} Pipeline", name));
    device.setObjectName(*m_pipelineLayout, fmt::format("{} Pipeline Layout", name));
    for (uint32_t setIndex = 0; setIndex < m_pipelineLayout->getDescriptorSetLayoutCount(); ++setIndex) {
        device.setObjectName(
            m_pipelineLayout->getDescriptorSetLayout(setIndex),
            fmt::format("{} Descriptor Set {} Layout", name, setIndex));
    }
}

VulkanDescriptorSet VulkanPipeline::allocateDescriptorSet(uint32_t setId) const {
    return {setId, m_pipelineLayout.get()};
}

void VulkanPipeline::swapAll(VulkanPipeline& other) {
    swap(other);
    m_pipelineLayout->swap(*other.m_pipelineLayout);
}
} // namespace crisp
