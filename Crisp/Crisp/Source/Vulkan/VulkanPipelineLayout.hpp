#pragma once

#include <vector>
#include <memory>

#include "VulkanResource.hpp"

namespace crisp
{
    class VulkanDevice;

    class VulkanPipelineLayout : public VulkanResource<VkPipelineLayout>
    {
    public:
        VulkanPipelineLayout(VulkanDevice* device, VkPipelineLayout pipelineLayoutHandle, std::vector<VkDescriptorSetLayout>&& setLayouts, std::vector<std::vector<VkDescriptorSetLayoutBinding>>&& setBindings, VkDescriptorPool descriptorPool);
        virtual ~VulkanPipelineLayout();

        inline VkDescriptorType getDescriptorType(uint32_t setIndex, uint32_t binding) const
        {
            return m_descriptorSetBindings[setIndex][binding].descriptorType;
        }

        VkDescriptorSet allocateSet(uint32_t setIndex) const;

    private:
        std::vector<VkDescriptorSetLayout>                     m_descriptorSetLayouts;
        std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_descriptorSetBindings;

        VkDescriptorPool m_descriptorPool;
    };

    class PipelineLayoutBuilder;
    std::unique_ptr<VulkanPipelineLayout> createPipelineLayout(VulkanDevice* device, PipelineLayoutBuilder& builder, VkDescriptorPool descriptorPool);
}