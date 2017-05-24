#include "DescriptorSetGroup.hpp"

#include "Vulkan/VulkanDevice.hpp"

namespace crisp
{
    DescriptorSetGroup::DescriptorSetGroup()
    {
    }

    DescriptorSetGroup::DescriptorSetGroup(std::initializer_list<VkDescriptorSet> setBindings)
        : m_sets(setBindings)
    {
    }

    DescriptorSetGroup::~DescriptorSetGroup()
    {
    }

    void DescriptorSetGroup::postBufferUpdate(uint32_t setIndex, uint32_t binding, VkDescriptorType type, VkDescriptorBufferInfo bufferInfo)
    {
        m_bufferInfos.emplace_back(bufferInfo);
        VkWriteDescriptorSet write = {};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = m_sets[setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = 0;
        write.descriptorType  = type;
        write.descriptorCount = 1;
        write.pBufferInfo     = &m_bufferInfos.back();

        m_writes.emplace_back(write);
    }

    void DescriptorSetGroup::postImageUpdate(uint32_t setIndex, uint32_t binding, VkDescriptorType type, VkDescriptorImageInfo imageInfo)
    {
        m_imageInfos.emplace_back(imageInfo);
        VkWriteDescriptorSet write = {};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = m_sets[setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = 0;
        write.descriptorType  = type;
        write.descriptorCount = 1;
        write.pImageInfo      = &m_imageInfos.back();

        m_writes.emplace_back(write);
    }

    void DescriptorSetGroup::postImageUpdate(uint32_t setIndex, uint32_t binding, uint32_t dstArrayElement, VkDescriptorType type, VkDescriptorImageInfo imageInfo)
    {
        m_imageInfos.emplace_back(imageInfo);
        VkWriteDescriptorSet write = {};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = m_sets[setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = dstArrayElement;
        write.descriptorType  = type;
        write.descriptorCount = 1;
        write.pImageInfo      = &m_imageInfos.back();

        m_writes.emplace_back(write);
    }

    void DescriptorSetGroup::flushUpdates(VulkanDevice* device)
    {
        vkUpdateDescriptorSets(device->getHandle(), static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
        m_writes.clear();
        m_imageInfos.clear();
        m_bufferInfos.clear();
    }

    void DescriptorSetGroup::setDynamicOffset(uint32_t index, uint32_t offset) const
    {
        if (index >= m_dynamicOffsets.size())
            m_dynamicOffsets.resize(index + 1);

        m_dynamicOffsets[index] = offset;
    }

    void DescriptorSetGroup::bind(VkCommandBuffer& cmdBuffer, VkPipelineLayout layout) const
    {
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
            0, static_cast<uint32_t>(m_sets.size()), m_sets.data(), static_cast<uint32_t>(m_dynamicOffsets.size()), m_dynamicOffsets.data());
    }

    void DescriptorSetGroup::bind(VkCommandBuffer& cmdBuffer, VkPipelineLayout layout, uint32_t firstSet, uint32_t numSets) const
    {
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
            firstSet, numSets, m_sets.data(), static_cast<uint32_t>(m_dynamicOffsets.size()), m_dynamicOffsets.data());
    }
}