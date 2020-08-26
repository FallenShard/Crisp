#pragma once

#include <vector>

#include "VulkanResource.hpp"

namespace crisp
{
    class VulkanDevice;

    class VulkanDescriptorSetLayout : public VulkanResource<VkDescriptorSetLayout>
    {
    public:
        VulkanDescriptorSetLayout(VulkanDevice* device, const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayoutCreateFlags flags = 0);
        ~VulkanDescriptorSetLayout();

        VkDescriptorType getDescriptorType(uint32_t bindingIndex) const;

        VkDescriptorSet allocateSet(VkDescriptorPool pool) const;
        std::vector<VkDescriptorSet> allocateSets(VkDescriptorPool pool, uint32_t count) const;

    private:
        std::vector<VkDescriptorSetLayoutBinding> m_descriptors;
    };
}