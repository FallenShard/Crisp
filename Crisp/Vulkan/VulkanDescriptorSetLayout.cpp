#include "VulkanDescriptorSetLayout.hpp"

#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice* device, const std::vector<VkDescriptorSetLayoutBinding>& descriptors, VkDescriptorSetLayoutCreateFlags flags)
        : VulkanResource(device)
    {
        uint32_t highestIndex = 0;
        for (auto& descriptor : descriptors)
        {
            if (descriptor.binding > highestIndex)
                highestIndex = descriptor.binding;
        }

        m_descriptors.resize(highestIndex + 1);
        for (auto& descriptor : descriptors)
        {
            m_descriptors[descriptor.binding] = descriptor;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptors.size());
        layoutInfo.pBindings    = descriptors.data();
        layoutInfo.flags        = flags;

        vkCreateDescriptorSetLayout(m_device->getHandle(), &layoutInfo, nullptr, &m_handle);
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        m_device->deferDestruction(m_framesToLive, m_handle, [](void* handle, VulkanDevice* device)
        {
            vkDestroyDescriptorSetLayout(device->getHandle(), static_cast<VkDescriptorSetLayout>(handle), nullptr);
        });
    }

    VkDescriptorType VulkanDescriptorSetLayout::getDescriptorType(uint32_t bindingIndex) const
    {
        return m_descriptors[bindingIndex].descriptorType;
    }

    VkDescriptorSet VulkanDescriptorSetLayout::allocateSet(VkDescriptorPool pool) const
    {
        VkDescriptorSetAllocateInfo descSetInfo = {};
        descSetInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descSetInfo.descriptorPool     = pool;
        descSetInfo.descriptorSetCount = 1;
        descSetInfo.pSetLayouts        = &m_handle;

        VkDescriptorSet descSet;
        vkAllocateDescriptorSets(m_device->getHandle(), &descSetInfo, &descSet);
        return descSet;
    }

    std::vector<VkDescriptorSet> VulkanDescriptorSetLayout::allocateSets(VkDescriptorPool pool, uint32_t count) const
    {
        std::vector<VkDescriptorSetLayout> layouts(count, m_handle);
        VkDescriptorSetAllocateInfo descSetInfo = {};
        descSetInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descSetInfo.descriptorPool     = pool;
        descSetInfo.descriptorSetCount = count;
        descSetInfo.pSetLayouts        = layouts.data();

        std::vector<VkDescriptorSet> descSets(count, nullptr);
        vkAllocateDescriptorSets(m_device->getHandle(), &descSetInfo, descSets.data());
        return descSets;
    }


}