#include "Vulkan/VulkanDescriptorSet.hpp"
#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    VulkanDescriptorSet::VulkanDescriptorSet(const VulkanPipeline* pipeline, uint32_t index)
        : m_layout(pipeline->getDescriptorSetLayout(index))
    {
        m_set = m_layout->allocateSet(pipeline->getDescriptorPool());
    }

    VkDescriptorType VulkanDescriptorSet::getDescriptorType(uint32_t index) const
    {
        return m_layout->getDescriptorType(index);
    }
}