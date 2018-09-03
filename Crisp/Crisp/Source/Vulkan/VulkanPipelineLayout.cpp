#include "VulkanPipelineLayout.hpp"

#include "VulkanDevice.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"

#include <CrispCore/Log.hpp>

namespace crisp
{
    VulkanPipelineLayout::VulkanPipelineLayout(VulkanDevice* device, VkPipelineLayout pipelineLayoutHandle, std::vector<VkDescriptorSetLayout>&& setLayouts,
        std::vector<std::vector<VkDescriptorSetLayoutBinding>>&& setBindings, std::vector<VkPushConstantRange>&& pushConstants, VkDescriptorPool descriptorPool)
        : VulkanResource(device, pipelineLayoutHandle)
        , m_descriptorSetLayouts(std::move(setLayouts))
        , m_descriptorSetBindings(std::move(setBindings))
        , m_pushConstants(std::move(pushConstants))
        , m_descriptorPool(descriptorPool)
    {
    }

    VulkanPipelineLayout::~VulkanPipelineLayout()
    {
        for (auto& setLayout : m_descriptorSetLayouts)
            vkDestroyDescriptorSetLayout(m_device->getHandle(), setLayout, nullptr);

        vkDestroyPipelineLayout(m_device->getHandle(), m_handle, nullptr);
        vkDestroyDescriptorPool(m_device->getHandle(), m_descriptorPool, nullptr);
    }

    VkDescriptorSet VulkanPipelineLayout::allocateSet(uint32_t setIndex) const
    {
        VkDescriptorSetAllocateInfo descSetInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        descSetInfo.descriptorPool     = m_descriptorPool;
        descSetInfo.descriptorSetCount = 1;
        descSetInfo.pSetLayouts        = &m_descriptorSetLayouts.at(setIndex);

        VkDescriptorSet descSet;
        vkAllocateDescriptorSets(m_device->getHandle(), &descSetInfo, &descSet);
        if (descSet == 0)
            logError("Descriptor set is nullptr!");
        return descSet;
    }

    std::unique_ptr<VulkanPipelineLayout> createPipelineLayout(VulkanDevice* device, PipelineLayoutBuilder& builder, VkDescriptorPool descriptorPool)
    {
        VkDevice deviceHandle = device->getHandle();
        auto pipelineLayout   = builder.create(deviceHandle);
        auto setBindings      = builder.extractDescriptorSetBindings();
        auto setLayouts       = builder.extractDescriptorSetLayouts();
        auto pushConstants    = builder.extractPushConstants();
        return std::make_unique<VulkanPipelineLayout>(device, pipelineLayout, std::move(setLayouts), std::move(setBindings), std::move(pushConstants), descriptorPool);
    }
}
