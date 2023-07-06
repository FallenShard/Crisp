#include <Crisp/Renderer/Material.hpp>

namespace crisp
{
Material::Material(VulkanPipeline* pipeline)
    : Material(pipeline, pipeline->getPipelineLayout()->getDescriptorSetAllocator())
{
}

Material::Material(VulkanPipeline* pipeline, DescriptorSetAllocator* descriptorSetAllocator)
    : m_pipeline(pipeline)
    , m_device(const_cast<VulkanDevice*>(&descriptorSetAllocator->getDevice()))
{
    const std::size_t setCount = m_pipeline->getPipelineLayout()->getDescriptorSetLayoutCount();

    for (auto& setVector : m_sets)
    {
        setVector.resize(setCount);
    }

    for (uint32_t i = 0; i < setCount; ++i)
    {
        if (m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(i))
        {
            for (uint32_t j = 0; j < m_sets.size(); ++j)
            {
                const VkDescriptorSetLayout setLayout = m_pipeline->getPipelineLayout()->getDescriptorSetLayout(i);
                m_sets[j][i] = descriptorSetAllocator->allocate(
                    setLayout, m_pipeline->getPipelineLayout()->getDescriptorSetLayoutBindings(i));
            }
        }
        else
        {
            const VkDescriptorSetLayout setLayout = m_pipeline->getPipelineLayout()->getDescriptorSetLayout(i);
            const VkDescriptorSet set = descriptorSetAllocator->allocate(
                setLayout, m_pipeline->getPipelineLayout()->getDescriptorSetLayoutBindings(i));
            for (uint32_t j = 0; j < m_sets.size(); ++j)
            {
                m_sets[j][i] = set;
            }
        }
    }

    m_dynamicBufferViews.resize(m_pipeline->getPipelineLayout()->getDynamicBufferCount());

    for (auto& perFrameOffsets : m_dynamicOffsets)
    {
        perFrameOffsets.resize(m_dynamicBufferViews.size());
    }
}

void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, VkDescriptorImageInfo&& imageInfo)
{
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setIndex];
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_device->postDescriptorWrite(std::move(write), imageInfo);
    }
}

void Material::writeDescriptor(
    uint32_t setIndex, uint32_t binding, const VulkanImageView& imageView, const VulkanSampler* sampler)
{
    writeDescriptor(setIndex, binding, imageView.getDescriptorInfo(sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

void Material::writeDescriptor(
    uint32_t setIndex, uint32_t binding, const VulkanImageView& imageView, const VulkanSampler& sampler)
{
    writeDescriptor(setIndex, binding, imageView.getDescriptorInfo(&sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

void Material::writeDescriptor(
    uint32_t setId,
    uint32_t binding,
    const VulkanRenderPass& renderPass,
    uint32_t renderTargetIndex,
    const VulkanSampler* sampler)
{
    uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setId) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setId];
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = m_pipeline->getDescriptorType(setId, binding);
        write.descriptorCount = 1;
        m_device->postDescriptorWrite(
            std::move(write), renderPass.getAttachmentView(renderTargetIndex, i).getDescriptorInfo(sampler));
    }
}

void Material::writeDescriptor(
    uint32_t setIndex,
    uint32_t binding,
    const std::vector<std::unique_ptr<VulkanImageView>>& imageViews,
    const VulkanSampler* sampler,
    VkImageLayout imageLayout)
{
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setIndex];
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_device->postDescriptorWrite(std::move(write), imageViews[i]->getDescriptorInfo(sampler, imageLayout));
    }
}

void Material::writeDescriptor(
    uint32_t setIndex,
    uint32_t binding,
    const std::vector<VulkanImageView*>& imageViews,
    const VulkanSampler* sampler,
    VkImageLayout imageLayout)
{
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setIndex];
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_device->postDescriptorWrite(std::move(write), imageViews[i]->getDescriptorInfo(sampler, imageLayout));
    }
}

void Material::writeDescriptor(
    uint32_t setIndex,
    uint32_t binding,
    uint32_t frameIdx,
    const VulkanImageView& imageView,
    const VulkanSampler* sampler)
{
    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = m_sets[frameIdx][setIndex];
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
    write.descriptorCount = 1;
    m_device->postDescriptorWrite(std::move(write), imageView.getDescriptorInfo(sampler));
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const uint32_t frameIndex,
    const uint32_t arrayIndex,
    const VulkanImageView& imageView,
    const VulkanSampler* sampler)
{
    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = m_sets[frameIndex][setIndex];
    write.dstBinding = binding;
    write.dstArrayElement = arrayIndex;
    write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
    write.descriptorCount = 1;
    m_device->postDescriptorWrite(std::move(write), imageView.getDescriptorInfo(sampler));
}

void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, VkDescriptorBufferInfo&& bufferInfo)
{
    uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setIndex];
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_device->postDescriptorWrite(std::move(write), bufferInfo);
    }
}

void Material::writeDescriptor(
    uint32_t setIndex, uint32_t binding, VkDescriptorBufferInfo&& bufferInfo, uint32_t dstElement)
{
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setIndex];
        write.dstBinding = binding;
        write.dstArrayElement = dstElement;
        write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_device->postDescriptorWrite(std::move(write), bufferInfo);
    }
}

void Material::writeDescriptor(uint32_t setId, uint32_t binding, const UniformBuffer& uniformBuffer)
{
    uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setId) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setId];
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = m_pipeline->getDescriptorType(setId, binding);
        write.descriptorCount = 1;
        m_device->postDescriptorWrite(std::move(write), uniformBuffer.getDescriptorInfo());
    }

    if (auto bufferIndex = m_pipeline->getPipelineLayout()->getDynamicBufferIndex(setId, binding); bufferIndex != -1)
    {
        setDynamicBufferView(bufferIndex, uniformBuffer, 0);
    }
}

void Material::writeDescriptor(
    uint32_t setIndex, uint32_t binding, const UniformBuffer& uniformBuffer, int elementSize, int elementCount)
{
    uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setIndex];
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = elementCount;

        std::vector<VkDescriptorBufferInfo> infos;
        infos.reserve(elementCount);
        for (int j = 0; j < elementCount; ++j)
        {
            infos.emplace_back(uniformBuffer.getDescriptorInfo(elementSize * j, elementSize));
        }

        m_device->postDescriptorWrite(std::move(write), std::move(infos));
    }
}

void Material::writeDescriptor(uint32_t setIndex, uint32_t binding, const StorageBuffer& storageBuffer)
{
    writeDescriptor(setIndex, binding, storageBuffer.getDescriptorInfo());
}

void Material::writeDescriptor(
    uint32_t setIndex, uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR& asInfo)
{
    uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i)
    {
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_sets[i][setIndex];
        write.pNext = &asInfo;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        m_device->postDescriptorWrite(std::move(write));
    }
}

void Material::setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset)
{
    m_dynamicOffsets[frameIdx][index] = offset;
}

void Material::bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint)
{
    if (!m_sets[frameIdx].empty())
    {
        vkCmdBindDescriptorSets(
            cmdBuffer,
            bindPoint,
            m_pipeline->getPipelineLayout()->getHandle(),
            0,
            static_cast<uint32_t>(m_sets[frameIdx].size()),
            m_sets[frameIdx].data(),
            static_cast<uint32_t>(m_dynamicOffsets[frameIdx].size()),
            m_dynamicOffsets[frameIdx].data());
    }
}

void Material::bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets)
{
    if (!m_sets[frameIdx].empty())
    {
        vkCmdBindDescriptorSets(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->getPipelineLayout()->getHandle(),
            0,
            static_cast<uint32_t>(m_sets[frameIdx].size()),
            m_sets[frameIdx].data(),
            static_cast<uint32_t>(dynamicBufferOffsets.size()),
            dynamicBufferOffsets.data());
    }
}

void Material::setDynamicBufferView(uint32_t index, const UniformBuffer& dynamicUniformBuffer, uint32_t offset)
{
    m_dynamicBufferViews.at(index) = {&dynamicUniformBuffer, offset};

    for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i)
    {
        m_dynamicOffsets.at(i).at(index) = dynamicUniformBuffer.getDynamicOffset(i);
    }
}
} // namespace crisp
