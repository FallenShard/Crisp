#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;

    class VulkanDescriptorSetLayout
    {
    public:
        VulkanDescriptorSetLayout(VulkanDevice* device, const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayoutCreateFlags flags = 0);
        ~VulkanDescriptorSetLayout();

        VkDescriptorType getDescriptorType(uint32_t bindingIndex) const;
        VkDescriptorSetLayout getHandle() const;

        VkDescriptorSet allocateSet(VkDescriptorPool pool) const;
        std::vector<VkDescriptorSet> allocateSets(VkDescriptorPool pool, uint32_t count) const;

    private:
        VulkanDevice* m_device;
        VkDescriptorSetLayout m_setLayout;

        std::vector<VkDescriptorSetLayoutBinding> m_descriptors;
    };
}