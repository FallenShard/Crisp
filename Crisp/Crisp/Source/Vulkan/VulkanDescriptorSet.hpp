#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanPipeline;
    class VulkanDescriptorSetLayout;

    class VulkanDescriptorSet
    {
    public:
        VulkanDescriptorSet(const VulkanPipeline* pipeline, uint32_t index);

        inline VkDescriptorSet            getHandle() const { return m_set; }
        inline VulkanDescriptorSetLayout* getLayout() const { return m_layout; }

        VkDescriptorType getDescriptorType(uint32_t index) const;

    private:
        VulkanDescriptorSetLayout* m_layout;
        VkDescriptorSet            m_set;
    };
}