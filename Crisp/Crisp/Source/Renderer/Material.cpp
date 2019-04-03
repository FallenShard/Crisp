#include "Material.hpp"

#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    Material::Material(VulkanPipeline* pipeline)
        : m_pipeline(pipeline)
    {
        std::size_t setCount = m_pipeline->getPipelineLayout()->getDescriptorSetLayoutCount();

        for (auto& setVector : m_sets)
            setVector.resize(setCount);

        for (std::size_t i = 0; i < setCount; ++i)
        {
            if (m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(i))
            {
                for (uint32_t j = 0; j < m_sets.size(); ++j)
                    m_sets[j][i] = pipeline->allocateDescriptorSet(i).getHandle();
            }
            else
            {
                VkDescriptorSet set = pipeline->allocateDescriptorSet(i).getHandle();
                for (uint32_t j = 0; j < m_sets.size(); ++j)
                    m_sets[j][i] = set;
            }
        }
    }

    Material::Material(VulkanPipeline* pipeline, std::vector<uint32_t> constantSetIds, std::vector<uint32_t> bufferedSetIds)
        : m_pipeline(pipeline)
    {
        std::vector<VkDescriptorSet> sharedSets;
        for (auto setId : constantSetIds)
        {
            if (setId >= sharedSets.size())
                sharedSets.resize(setId + 1);

            sharedSets[setId] = pipeline->allocateDescriptorSet(setId).getHandle();
        }

        for (auto& setVec : m_sets)
        {
            setVec = sharedSets;
            for (auto setId : bufferedSetIds)
            {
                if (setId >= setVec.size())
                    setVec.resize(setId + 1);

                setVec[setId] = pipeline->allocateDescriptorSet(setId).getHandle();
            }
        }
    }

    VkWriteDescriptorSet Material::makeDescriptorWrite(uint32_t setIndex, uint32_t binding, uint32_t frameIdx)
    {
        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet          = m_sets[frameIdx][setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = 0;
        write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        return write;
    }

    VkWriteDescriptorSet Material::makeDescriptorWrite(uint32_t setIdx, uint32_t binding, uint32_t index, uint32_t frameIdx)
    {
        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet          = m_sets[frameIdx][setIdx];
        write.dstBinding      = binding;
        write.dstArrayElement = index;
        write.descriptorType  = m_pipeline->getDescriptorType(setIdx, binding);
        write.descriptorCount = 1;
        return write;
    }

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, VkDescriptorBufferInfo&& bufferInfo)
    {
        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet          = m_sets[frameIdx][setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = 0;
        write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_pipeline->getDevice()->postDescriptorWrite(std::move(write), std::forward<VkDescriptorBufferInfo>(bufferInfo));
    }

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, VkDescriptorImageInfo&& imageInfo)
    {
        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet          = m_sets[frameIdx][setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = 0;
        write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_pipeline->getDevice()->postDescriptorWrite(std::move(write), std::forward<VkDescriptorImageInfo>(imageInfo));
    }

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, const VulkanImageView& imageView, VkSampler sampler)
    {
        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet          = m_sets[frameIdx][setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = 0;
        write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_pipeline->getDevice()->postDescriptorWrite(std::move(write), imageView.getDescriptorInfo(sampler));
    }

    void Material::writeDescriptor(uint32_t setIdx, uint32_t binding, uint32_t arrayIndex, uint32_t frameIdx)
    {
    }

    void Material::setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset)
    {
        if (m_dynamicOffsets[frameIdx].size() <= index)
            m_dynamicOffsets[frameIdx].resize(index + 1);

        m_dynamicOffsets[frameIdx][index] = offset;
    }

    void Material::bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer)
    {
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout()->getHandle(),
            0, static_cast<uint32_t>(m_sets[frameIdx].size()), m_sets[frameIdx].data(),
            static_cast<uint32_t>(m_dynamicOffsets[frameIdx].size()), m_dynamicOffsets[frameIdx].data());
    }

    void Material::addDynamicBufferInfo(const UniformBuffer& dynamicUniformBuffer, uint32_t offset)
    {
        m_dynamicBufferInfos.push_back({ dynamicUniformBuffer, offset });
    }
}