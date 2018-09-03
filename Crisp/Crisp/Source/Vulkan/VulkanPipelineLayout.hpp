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
        VulkanPipelineLayout(VulkanDevice* device, VkPipelineLayout pipelineLayoutHandle, std::vector<VkDescriptorSetLayout>&& setLayouts,
            std::vector<std::vector<VkDescriptorSetLayoutBinding>>&& setBindings, std::vector<VkPushConstantRange>&& pushConstants, VkDescriptorPool descriptorPool);
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

    private:
        std::vector<VkDescriptorSetLayout>                     m_descriptorSetLayouts;
        std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_descriptorSetBindings;
        std::vector<VkPushConstantRange>                       m_pushConstants;

        VkDescriptorPool m_descriptorPool;
    };

    class PipelineLayoutBuilder;
    std::unique_ptr<VulkanPipelineLayout> createPipelineLayout(VulkanDevice* device, PipelineLayoutBuilder& builder, VkDescriptorPool descriptorPool);
}