#include "Material.hpp"

#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    Material::Material(VulkanPipeline* pipeline)
        : m_pipeline(pipeline)
    {
        std::size_t setCount = m_pipeline->getPipelineLayout()->getDescriptorSetLayoutCount();

        for (auto& setVector : m_sets)
            setVector.resize(setCount);

        for (uint32_t i = 0; i < setCount; ++i)
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

        m_dynamicBufferViews.resize(m_pipeline->getPipelineLayout()->getDynamicBufferCount());

        for (auto& perFrameOffsets : m_dynamicOffsets)
            perFrameOffsets.resize(m_dynamicBufferViews.size());
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

    void Material::writeDescriptor(uint32_t setId, uint32_t binding, const VulkanRenderPass& renderPass, uint32_t renderTargetIndex, const VulkanSampler* sampler)
    {
        uint32_t setsToUpdate = m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setId) ? Renderer::NumVirtualFrames : 1;
        for (uint32_t i = 0; i < setsToUpdate; ++i)
        {
            VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet          = m_sets[i][setId];
            write.dstBinding      = binding;
            write.dstArrayElement = 0;
            write.descriptorType  = m_pipeline->getDescriptorType(setId, binding);
            write.descriptorCount = 1;
            m_pipeline->getDevice()->postDescriptorWrite(std::move(write), renderPass.getRenderTargetView(renderTargetIndex, i).getDescriptorInfo(sampler));
        }
    }

    void Material::writeDescriptor(uint32_t setId, uint32_t binding, const UniformBuffer& uniformBuffer)
    {
        uint32_t setsToUpdate = m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setId) ? Renderer::NumVirtualFrames : 1;
        for (uint32_t i = 0; i < setsToUpdate; ++i)
        {
            VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet          = m_sets[i][setId];
            write.dstBinding      = binding;
            write.dstArrayElement = 0;
            write.descriptorType  = m_pipeline->getDescriptorType(setId, binding);
            write.descriptorCount = 1;
            m_pipeline->getDevice()->postDescriptorWrite(std::move(write), uniformBuffer.getDescriptorInfo());
        }
    }

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, const UniformBuffer& uniformBuffer, int elementSize, int elementCount)
    {
        uint32_t setsToUpdate = m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? Renderer::NumVirtualFrames : 1;
        for (uint32_t i = 0; i < setsToUpdate; ++i)
        {
            VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet          = m_sets[i][setIndex];
            write.dstBinding      = binding;
            write.dstArrayElement = 0;
            write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
            write.descriptorCount = elementCount;

            std::vector<VkDescriptorBufferInfo> infos;
            infos.reserve(elementCount);
            for (int j = 0; j < elementCount; ++j)
                infos.emplace_back(uniformBuffer.getDescriptorInfo(elementSize * j, elementSize));

            m_pipeline->getDevice()->postDescriptorWrite(std::move(write), std::move(infos));
        }
    }

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, VkDescriptorBufferInfo&& bufferInfo)
    {
        uint32_t setsToUpdate = m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? Renderer::NumVirtualFrames : 1;
        for (uint32_t i = 0; i < setsToUpdate; ++i)
        {
            VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet          = m_sets[i][setIndex];
            write.dstBinding      = binding;
            write.dstArrayElement = 0;
            write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
            write.descriptorCount = 1;
            m_pipeline->getDevice()->postDescriptorWrite(std::move(write), bufferInfo);
        }
    }

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanImageView& imageView, const VulkanSampler* sampler)
    {
        writeDescriptor(setIndex, binding, imageView, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanImageView& imageView, const VulkanSampler* sampler, VkImageLayout imageLayout)
    {
        uint32_t setsToUpdate = m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? Renderer::NumVirtualFrames : 1;
        for (uint32_t i = 0; i < setsToUpdate; ++i)
        {
            VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet          = m_sets[i][setIndex];
            write.dstBinding      = binding;
            write.dstArrayElement = 0;
            write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
            write.descriptorCount = 1;
            m_pipeline->getDevice()->postDescriptorWrite(std::move(write), imageView.getDescriptorInfo(sampler, imageLayout));
        }
    }

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, const std::vector<std::unique_ptr<VulkanImageView>>& imageViews, const VulkanSampler* sampler, VkImageLayout imageLayout)
    {
        uint32_t setsToUpdate = m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? Renderer::NumVirtualFrames : 1;
        for (uint32_t i = 0; i < setsToUpdate; ++i)
        {
            VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet          = m_sets[i][setIndex];
            write.dstBinding      = binding;
            write.dstArrayElement = 0;
            write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
            write.descriptorCount = 1;
            m_pipeline->getDevice()->postDescriptorWrite(std::move(write), imageViews[i]->getDescriptorInfo(sampler, imageLayout));
        }
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

    void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, const VulkanImageView& imageView, const VulkanSampler* sampler)
    {
        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet          = m_sets[frameIdx][setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = 0;
        write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_pipeline->getDevice()->postDescriptorWrite(std::move(write), imageView.getDescriptorInfo(sampler));
    }

    void Material::setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset)
    {
        m_dynamicOffsets[frameIdx][index] = offset;
    }

    void Material::bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint)
    {
        vkCmdBindDescriptorSets(cmdBuffer, bindPoint, m_pipeline->getPipelineLayout()->getHandle(),
            0, static_cast<uint32_t>(m_sets[frameIdx].size()), m_sets[frameIdx].data(),
            static_cast<uint32_t>(m_dynamicOffsets[frameIdx].size()), m_dynamicOffsets[frameIdx].data());
    }

    void Material::bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets)
    {
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout()->getHandle(),
            0, static_cast<uint32_t>(m_sets[frameIdx].size()), m_sets[frameIdx].data(),
            static_cast<uint32_t>(dynamicBufferOffsets.size()), dynamicBufferOffsets.data());
    }

    void Material::setDynamicBufferView(uint32_t index, const UniformBuffer& dynamicUniformBuffer, uint32_t offset)
    {
        m_dynamicBufferViews.at(index) = { &dynamicUniformBuffer, offset };

        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
            m_dynamicOffsets.at(i).at(index) = dynamicUniformBuffer.getDynamicOffset(i);
    }
}