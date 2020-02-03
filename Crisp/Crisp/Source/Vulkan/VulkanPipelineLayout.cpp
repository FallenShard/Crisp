#include "VulkanPipelineLayout.hpp"

#include "VulkanDevice.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"

#include <CrispCore/Log.hpp>

#include "Renderer/DescriptorSetAllocator.hpp"

namespace crisp
{
    namespace
    {
        VkPipelineLayout createHandle(VkDevice device, const std::vector<VkDescriptorSetLayout>& setLayouts, const std::vector<VkPushConstantRange>& pushConstants)
        {
            VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
            pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
            pipelineLayoutInfo.pSetLayouts            = setLayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
            pipelineLayoutInfo.pPushConstantRanges    = pushConstants.data();

            VkPipelineLayout layout;
            vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);
            return layout;
        }
    }

    VulkanPipelineLayout::VulkanPipelineLayout(VulkanDevice* device, std::vector<VkDescriptorSetLayout>&& setLayouts,
        std::vector<std::vector<VkDescriptorSetLayoutBinding>>&& setBindings, std::vector<VkPushConstantRange>&& pushConstants,
        std::vector<bool> descriptorSetBufferedStatus, std::unique_ptr<DescriptorSetAllocator> setAllocator)
        : VulkanResource(device, createHandle(device->getHandle(), setLayouts, pushConstants))
        , m_descriptorSetLayouts(std::move(setLayouts))
        , m_descriptorSetBindings(std::move(setBindings))
        , m_pushConstants(std::move(pushConstants))
        , m_descriptorSetBufferedStatus(descriptorSetBufferedStatus)
        , m_dynamicBufferCount(0)
        , m_setAllocator(std::move(setAllocator))
    {
        for (const auto& bindingSet : m_descriptorSetBindings)
        {
            for (const auto& binding : bindingSet)
                if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                    binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                    ++m_dynamicBufferCount;
        }
    }

    VulkanPipelineLayout::~VulkanPipelineLayout()
    {
        for (auto& setLayout : m_descriptorSetLayouts)
            vkDestroyDescriptorSetLayout(m_device->getHandle(), setLayout, nullptr);

        vkDestroyPipelineLayout(m_device->getHandle(), m_handle, nullptr);
    }

    VkDescriptorSet VulkanPipelineLayout::allocateSet(uint32_t setIndex) const
    {
        return m_setAllocator->allocate(m_descriptorSetLayouts.at(setIndex), m_descriptorSetBindings.at(setIndex));
    }
}
