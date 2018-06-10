#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanPipelineLayout;

    class VulkanDescriptorSet
    {
    public:
        VulkanDescriptorSet(VkDescriptorSet set, uint32_t index, const VulkanPipelineLayout* pipelineLayout);

        inline VkDescriptorSet getHandle() const { return m_set; }

        VkDescriptorType getDescriptorType(uint32_t index) const;

    private:
        VkDescriptorSet             m_set;
        uint32_t                    m_index;
        const VulkanPipelineLayout* m_pipelinelayout;
    };
}