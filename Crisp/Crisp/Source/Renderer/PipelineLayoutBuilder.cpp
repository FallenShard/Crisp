#include "PipelineLayoutBuilder.hpp"

#include "Renderer/Renderer.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanPipelineLayout.hpp"

#include "ShadingLanguage/Reflection.hpp"

#include <numeric>

namespace crisp
{
    std::vector<VkDescriptorPoolSize> calculateMinimumPoolSizes(const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& setBindings, const std::vector<uint32_t>& numCopies)
    {
        std::vector<uint32_t> numSetCopies = numCopies;
        if (numSetCopies.size() == 0)
            numSetCopies.resize(setBindings.size(), 1);

        std::vector<VkDescriptorPoolSize> poolSizes;
        for (uint32_t i = 0; i < setBindings.size(); i++)
        {
            const std::vector<VkDescriptorSetLayoutBinding>& setBinding = setBindings.at(i);
            for (const auto& descriptorBinding : setBinding)
            {
                uint32_t k = 0;
                while (k < poolSizes.size())
                {
                    if (poolSizes[k].type == descriptorBinding.descriptorType)
                        break;
                    k++;
                }


                if (k == poolSizes.size())
                    poolSizes.push_back({ descriptorBinding.descriptorType, 0 });

                poolSizes[k].descriptorCount += descriptorBinding.descriptorCount * numSetCopies.at(i);
            }
        }
        return poolSizes;
    }

    VkDescriptorPool createDescriptorPool(VkDevice device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSetCount, VkDescriptorPoolCreateFlags flags)
    {
        if (poolSizes.empty())
            return nullptr;

        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.flags         = flags;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        poolInfo.maxSets       = maxSetCount;

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
        return descriptorPool;
    }

    VkDescriptorPool createDescriptorPool(VkDevice device, const PipelineLayoutBuilder& builder, const std::vector<uint32_t>& numCopies, uint32_t maxSetCount, VkDescriptorPoolCreateFlags flags)
    {
        return createDescriptorPool(device, calculateMinimumPoolSizes(builder.getDescriptorSetBindings(), numCopies), maxSetCount, flags);
    }

    PipelineLayoutBuilder::PipelineLayoutBuilder(const sl::Reflection& shaderReflection)
    {
        for (int setId = 0; setId < shaderReflection.getDescriptorSetCount(); ++setId)
            defineDescriptorSet(setId, shaderReflection.isSetBuffered(setId),
                shaderReflection.getDescriptorSetLayouts(setId));

        for (const auto& pc : shaderReflection.getPushConstants())
            addPushConstant(pc.stageFlags, pc.offset, pc.size);
    }

    PipelineLayoutBuilder& PipelineLayoutBuilder::defineDescriptorSet(uint32_t set, std::vector<VkDescriptorSetLayoutBinding>&& bindings, VkDescriptorSetLayoutCreateFlags flags)
    {
        if (m_setBindings.size() <= set)
        {
            std::size_t newSize = static_cast<std::size_t>(set) + 1;
            m_setBindings.resize(newSize);
            m_setCreateInfos.resize(newSize);
            m_setBuffered.resize(newSize);
        }

        m_setBindings[set] = std::move(bindings);
        m_setCreateInfos[set] = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        m_setCreateInfos[set].bindingCount = static_cast<uint32_t>(m_setBindings[set].size());
        m_setCreateInfos[set].pBindings    = m_setBindings[set].data();
        m_setCreateInfos[set].flags        = flags;
        return *this;
    }

    PipelineLayoutBuilder& PipelineLayoutBuilder::defineDescriptorSet(uint32_t set, bool isBuffered, std::vector<VkDescriptorSetLayoutBinding>&& bindings, VkDescriptorSetLayoutCreateFlags flags)
    {
        if (m_setBindings.size() <= set)
        {
            std::size_t newSize = static_cast<std::size_t>(set) + 1;
            m_setBindings.resize(newSize);
            m_setCreateInfos.resize(newSize);
            m_setBuffered.resize(newSize);
        }

        m_setBindings[set] = std::move(bindings);
        m_setCreateInfos[set] = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        m_setCreateInfos[set].bindingCount = static_cast<uint32_t>(m_setBindings[set].size());
        m_setCreateInfos[set].pBindings = m_setBindings[set].data();
        m_setCreateInfos[set].flags = flags;
        m_setBuffered[set] = isBuffered;
        return *this;
    }

    PipelineLayoutBuilder& PipelineLayoutBuilder::addPushConstant(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size)
    {
        m_pushConstants.push_back({ stageFlags, offset, size });
        return *this;
    }

    VkPipelineLayout PipelineLayoutBuilder::create(VkDevice device)
    {
        m_setLayouts.resize(m_setCreateInfos.size(), VK_NULL_HANDLE);
        for (uint32_t i = 0; i < m_setCreateInfos.size(); i++)
        {
            vkCreateDescriptorSetLayout(device, &m_setCreateInfos[i], nullptr, &m_setLayouts[i]);
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(m_setLayouts.size());
        pipelineLayoutInfo.pSetLayouts            = m_setLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstants.size());
        pipelineLayoutInfo.pPushConstantRanges    = m_pushConstants.data();

        VkPipelineLayout layout;
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);
        return layout;
    }

    std::unique_ptr<VulkanPipelineLayout> PipelineLayoutBuilder::create(VulkanDevice* device, VkDescriptorPool descriptorPool)
    {
        return std::make_unique<VulkanPipelineLayout>(device, create(device->getHandle()),
            std::move(m_setLayouts), std::move(m_setBindings), std::move(m_pushConstants), std::move(m_setBuffered), descriptorPool);
    }

    std::unique_ptr<VulkanPipelineLayout> PipelineLayoutBuilder::create(VulkanDevice* device, uint32_t numCopies, VkDescriptorPoolCreateFlags flags)
    {
        return create(device, createMinimalDescriptorPool(device, numCopies, flags));
    }

    VkDescriptorPool PipelineLayoutBuilder::createMinimalDescriptorPool(VulkanDevice* device, uint32_t numCopies, VkDescriptorPoolCreateFlags flags) const
    {
        std::vector<uint32_t> numCopiesPerSet = getNumCopiesPerSet(numCopies);
        uint32_t maxSetCount = std::accumulate(numCopiesPerSet.begin(), numCopiesPerSet.end(), 0u);
        return createDescriptorPool(device->getHandle(), calculateMinimumPoolSizes(getDescriptorSetBindings(), numCopiesPerSet), maxSetCount, flags);
    }

    std::vector<VkDescriptorSetLayout> PipelineLayoutBuilder::extractDescriptorSetLayouts()
    {
        return std::move(m_setLayouts);
    }

    std::vector<std::vector<VkDescriptorSetLayoutBinding>> PipelineLayoutBuilder::extractDescriptorSetBindings()
    {
        return std::move(m_setBindings);
    }

    std::vector<VkPushConstantRange> PipelineLayoutBuilder::extractPushConstants()
    {
        return std::move(m_pushConstants);
    }

    const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& PipelineLayoutBuilder::getDescriptorSetBindings() const
    {
        return m_setBindings;
    }

    std::vector<uint32_t> PipelineLayoutBuilder::getNumCopiesPerSet(uint32_t numCopies) const
    {
        std::vector<uint32_t> numCopiesPerSet;
        for (uint32_t i = 0; i < m_setBuffered.size(); ++i)
            numCopiesPerSet.push_back(m_setBuffered[i] ? numCopies * Renderer::NumVirtualFrames : numCopies);
        return numCopiesPerSet;
    }
}