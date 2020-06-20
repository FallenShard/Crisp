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
        m_dynamicBufferIndices.resize(m_descriptorSetBindings.size());
        for (std::size_t s = 0; s < m_descriptorSetBindings.size(); ++s)
        {
            m_dynamicBufferIndices[s].resize(m_descriptorSetBindings[s].size(), -1);
            for (std::size_t b = 0; b < m_descriptorSetBindings[s].size(); ++b)
                if (m_descriptorSetBindings[s][b].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                    m_descriptorSetBindings[s][b].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                    m_dynamicBufferIndices[s][b] = m_dynamicBufferCount++;
        }
    }

    VulkanPipelineLayout::~VulkanPipelineLayout()
    {
        if (m_deferDestruction)
        {
            m_device->deferDestruction([h = m_handle, sets = m_descriptorSetLayouts](VkDevice device)
                {
                    for (auto& setLayout : sets)
                        vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
                    vkDestroyPipelineLayout(device, h, nullptr);
                });
        }
        else
        {
            for (auto& setLayout : m_descriptorSetLayouts)
                vkDestroyDescriptorSetLayout(m_device->getHandle(), setLayout, nullptr);
            vkDestroyPipelineLayout(m_device->getHandle(), m_handle, nullptr);
        }
    }

    VkDescriptorSet VulkanPipelineLayout::allocateSet(uint32_t setIndex) const
    {
        return m_setAllocator->allocate(m_descriptorSetLayouts.at(setIndex), m_descriptorSetBindings.at(setIndex));
    }

    uint32_t VulkanPipelineLayout::getDynamicBufferIndex(uint32_t setIndex, uint32_t binding) const
    {
        return m_dynamicBufferIndices.at(setIndex).at(binding);
    }

    void VulkanPipelineLayout::swap(VulkanPipelineLayout& other)
    {
        /*std::vector<VkDescriptorSetLayout>                     m_descriptorSetLayouts;
        std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_descriptorSetBindings;
        std::vector<VkPushConstantRange>                       m_pushConstants;
        std::vector<bool> m_descriptorSetBufferedStatus;

        std::vector<std::vector<uint32_t>> m_dynamicBufferIndices;
        std::size_t m_dynamicBufferCount;

        std::unique_ptr<DescriptorSetAllocator> m_setAllocator;*/
    }
}
