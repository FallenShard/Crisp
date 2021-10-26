#include "VulkanPipelineLayout.hpp"

#include "VulkanDevice.hpp"
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

#include <Crisp/Renderer/DescriptorSetAllocator.hpp>
#include <Crisp/Renderer/Renderer.hpp>

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
        for (auto setLayout : m_descriptorSetLayouts)
        {
            m_device->deferDestruction(m_framesToLive, setLayout, [](void* handle, VulkanDevice* device)
            {
                spdlog::debug("Destroying set layout: {}", handle);
                vkDestroyDescriptorSetLayout(device->getHandle(), static_cast<VkDescriptorSetLayout>(handle), nullptr);
            });
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
        std::swap(m_descriptorSetLayouts, other.m_descriptorSetLayouts);
        std::swap(m_descriptorSetBindings, other.m_descriptorSetBindings);
        std::swap(m_pushConstants, other.m_pushConstants);
        std::swap(m_descriptorSetBufferedStatus, other.m_descriptorSetBufferedStatus);

        std::swap(m_dynamicBufferIndices, other.m_dynamicBufferIndices);
        std::swap(m_dynamicBufferCount, other.m_dynamicBufferCount);


        //std::unique_ptr<DescriptorSetAllocator> m_setAllocator;
    }

    std::unique_ptr<DescriptorSetAllocator> VulkanPipelineLayout::createDescriptorSetAllocator(uint32_t numCopies, VkDescriptorPoolCreateFlags flags)
    {
        auto getNumCopiesPerSet = [this](uint32_t numCopies) {
            std::vector<uint32_t> numCopiesPerSet;
            for (uint32_t i = 0; i < m_descriptorSetBufferedStatus.size(); ++i)
                numCopiesPerSet.push_back(m_descriptorSetBufferedStatus[i] ? numCopies * Renderer::NumVirtualFrames : numCopies);
            return numCopiesPerSet;
        };

        return std::make_unique<DescriptorSetAllocator>(m_device, m_descriptorSetBindings, getNumCopiesPerSet(numCopies), flags);
    }
}
