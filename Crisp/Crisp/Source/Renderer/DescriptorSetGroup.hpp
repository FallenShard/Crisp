#pragma once

#include <vector>
#include <list>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;

    class DescriptorSetGroup
    {
    public:
        DescriptorSetGroup();
        DescriptorSetGroup(std::initializer_list<VkDescriptorSet> setBindings);
        ~DescriptorSetGroup();

        void postBufferUpdate(uint32_t setIndex, uint32_t binding, VkDescriptorType type, VkDescriptorBufferInfo bufferInfo);
        void postImageUpdate(uint32_t setIndex, uint32_t binding, VkDescriptorType type, VkDescriptorImageInfo imageInfo);
        void postImageUpdate(uint32_t setIndex, uint32_t binding, uint32_t dstArrayElement, VkDescriptorType type, VkDescriptorImageInfo imageInfo);

        void flushUpdates(VulkanDevice* device);

        void setDynamicOffset(uint32_t index, uint32_t offset) const;

        void bind(VkCommandBuffer& cmdBuffer, VkPipelineLayout layout) const;
        void bind(VkCommandBuffer& cmdBuffer, VkPipelineLayout layout, uint32_t firstSet, uint32_t numSets) const;

    private:
        std::vector<VkDescriptorSet> m_sets;
        mutable std::vector<uint32_t> m_dynamicOffsets;

        std::list<VkDescriptorBufferInfo> m_bufferInfos;
        std::list<VkDescriptorImageInfo>  m_imageInfos;
        std::vector<VkWriteDescriptorSet> m_writes;
    };
}