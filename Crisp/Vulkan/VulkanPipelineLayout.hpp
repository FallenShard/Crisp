#pragma once

#include <vector>
#include <memory>

#include "VulkanResource.hpp"

namespace crisp
{
    class VulkanDevice;
    class DescriptorSetAllocator;

    class VulkanPipelineLayout : public VulkanResource<VkPipelineLayout>
    {
    public:
        VulkanPipelineLayout(VulkanDevice* device, std::vector<VkDescriptorSetLayout>&& setLayouts,
            std::vector<std::vector<VkDescriptorSetLayoutBinding>>&& setBindings, std::vector<VkPushConstantRange>&& pushConstants,
            std::vector<bool> descriptorSetBufferedStatus, std::unique_ptr<DescriptorSetAllocator> setAllocator);
        virtual ~VulkanPipelineLayout();

        inline VkDescriptorType getDescriptorType(uint32_t setIndex, uint32_t binding) const
        {
            return m_descriptorSetBindings[setIndex][binding].descriptorType;
        }

        VkDescriptorSet allocateSet(uint32_t setIndex) const;

        inline void setPushConstants(VkCommandBuffer cmdBuffer, const char* data) const
        {
            for (const auto& pushConstant : m_pushConstants)
                vkCmdPushConstants(cmdBuffer, m_handle, pushConstant.stageFlags, pushConstant.offset, pushConstant.size, data + pushConstant.offset);
        }

        inline std::size_t getDescriptorSetLayoutCount() const
        {
            return m_descriptorSetLayouts.size();
        }

        inline bool isDescriptorSetBuffered(uint32_t setIndex) const
        {
            return m_descriptorSetBufferedStatus[setIndex];
        }

        inline std::size_t getDynamicBufferCount() const
        {
            return m_dynamicBufferCount;
        }

        uint32_t getDynamicBufferIndex(uint32_t setIndex, uint32_t binding) const;

        void swap(VulkanPipelineLayout& other);

    private:
        std::vector<VkDescriptorSetLayout>                     m_descriptorSetLayouts;
        std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_descriptorSetBindings;
        std::vector<VkPushConstantRange>                       m_pushConstants;
        std::vector<bool> m_descriptorSetBufferedStatus;

        std::vector<std::vector<uint32_t>> m_dynamicBufferIndices;
        uint32_t m_dynamicBufferCount;

        std::unique_ptr<DescriptorSetAllocator> m_setAllocator;
    };

    //class PipelineLayoutBuilder;
    //std::unique_ptr<VulkanPipelineLayout> createPipelineLayout(VulkanDevice* device, PipelineLayoutBuilder& builder, VkDescriptorPool descriptorPool);
    //std::unique_ptr<VulkanPipelineLayout> createPipelineLayout(VulkanDevice* device, PipelineLayoutBuilder& builder, std::vector<bool> setBuffered, VkDescriptorPool descriptorPool);
}