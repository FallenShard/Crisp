#include <Crisp/Vulkan/VulkanDescriptorSet.hpp>

namespace crisp
{
VulkanDescriptorSet::VulkanDescriptorSet(uint32_t index, const VulkanPipelineLayout* pipelineLayout)
    : m_set(pipelineLayout->allocateSet(index))
    , m_index(index)
    , m_pipelinelayout(pipelineLayout)
{
}

VkDescriptorType VulkanDescriptorSet::getDescriptorType(uint32_t index) const
{
    return m_pipelinelayout->getDescriptorType(m_index, index);
}
} // namespace crisp