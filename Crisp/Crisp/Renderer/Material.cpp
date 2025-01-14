#include <Crisp/Renderer/Material.hpp>

namespace crisp {
Material::Material(VulkanPipeline* pipeline)
    : Material(
          pipeline,
          pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(),
          0,
          pipeline->getPipelineLayout()->getDescriptorSetLayoutCount()) {}

Material::Material(VulkanPipeline* pipeline, VulkanDescriptorSetAllocator* descriptorSetAllocator)
    : Material(pipeline, descriptorSetAllocator, 0, pipeline->getPipelineLayout()->getDescriptorSetLayoutCount()) {}

Material::Material(VulkanPipeline* pipeline, const uint32_t firstSet, const uint32_t setCount)
    : Material(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), firstSet, setCount) {}

Material::Material(
    VulkanPipeline* pipeline,
    VulkanDescriptorSetAllocator* descriptorSetAllocator,
    const uint32_t firstSet,
    const uint32_t setCount)
    : m_sets(setCount, VK_NULL_HANDLE)
    , m_firstSet(firstSet)
    , m_setCount(setCount)
    , m_device(const_cast<VulkanDevice*>(&descriptorSetAllocator->getDevice())) // NOLINT
    , m_pipeline(pipeline) {
    const auto& pipelineLayout{*m_pipeline->getPipelineLayout()};

    for (uint32_t setIdx = 0; setIdx < m_setCount; ++setIdx) {
        m_sets[setIdx] = descriptorSetAllocator->allocate(
            pipelineLayout.getDescriptorSetLayout(setIdx),
            pipelineLayout.getDescriptorSetLayoutBindings(setIdx),
            pipelineLayout.getDescriptorSetBindlessBindings(setIdx));
    }

    m_dynamicOffsetCount = 0;
    for (uint32_t i = m_firstSet; i < m_firstSet + m_setCount; ++i) {
        m_dynamicOffsetCount += pipelineLayout.getDynamicBufferCount(i);
    }
    m_dynamicOffsets.resize(m_dynamicOffsetCount);
}

void Material::writeDescriptor(const uint32_t setIndex, const uint32_t binding, const VkDescriptorImageInfo& imageInfo) {
    CRISP_CHECK_GE_LT(setIndex, 0, m_sets.size());
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_sets[setIndex],
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
        },
        imageInfo);
}

void Material::writeDescriptor(
    const uint32_t setIndex, VkWriteDescriptorSet write, const VkDescriptorImageInfo& imageInfo) {
    CRISP_CHECK_GE_LT(setIndex, 0, m_sets.size());
    write.dstSet = m_sets[setIndex];
    m_device->postDescriptorWrite(write, imageInfo);
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const VulkanImageView& imageView,
    const VulkanSampler& sampler,
    const std::optional<VkImageLayout> imageLayout) {
    writeDescriptor(
        setIndex,
        binding,
        imageView.getDescriptorInfo(&sampler, imageLayout.value_or(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)));
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const VulkanRenderPass& renderPass,
    const uint32_t renderTargetIndex,
    const VulkanSampler* sampler) {
    CRISP_CHECK_GE_LT(setIndex, 0, m_sets.size());
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_sets[setIndex],
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
        },
        renderPass.getAttachmentView(renderTargetIndex).getDescriptorInfo(sampler));
}

void Material::writeBindlessDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const uint32_t arrayIndex,
    const VulkanImageView& imageView,
    const VulkanSampler* sampler) {
    CRISP_CHECK_GE_LT(setIndex, 0, m_sets.size());
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_sets[setIndex],
            .dstBinding = binding,
            .dstArrayElement = arrayIndex,
            .descriptorCount = 1,
            .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
        },
        imageView.getDescriptorInfo(sampler));
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VkDescriptorBufferInfo& bufferInfo) {
    CRISP_CHECK_GE_LT(setIndex, 0, m_sets.size());
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_sets[setIndex],
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
        },
        bufferInfo);
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, const uint32_t dstElement) {
    CRISP_CHECK_GE_LT(setIndex, 0, m_sets.size());
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_sets[setIndex],
            .dstBinding = binding,
            .dstArrayElement = dstElement,
            .descriptorCount = 1,
            .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
        },
        bufferInfo);
}

void Material::writeDescriptor(const uint32_t setIndex, const uint32_t binding, const VulkanBuffer& buffer) {
    writeDescriptor(setIndex, binding, buffer.createDescriptorInfo());
}

void Material::writeDescriptor(const uint32_t setIndex, const uint32_t binding, const VulkanRingBuffer& buffer) {
    writeDescriptor(setIndex, binding, buffer.getDescriptorInfo());
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR& asInfo) {
    CRISP_CHECK_GE_LT(setIndex, 0, m_sets.size());
    m_device->postDescriptorWrite({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asInfo,
        .dstSet = m_sets[setIndex],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
    });
}

void Material::setDynamicOffset(const uint32_t index, const uint32_t offset) {
    CRISP_CHECK_GE_LT(index, 0, m_dynamicOffsets.size());
    m_dynamicOffsets[index] = offset;
}

void Material::bind(const VkCommandBuffer cmdBuffer) {
    vkCmdBindDescriptorSets(
        cmdBuffer,
        m_pipeline->getBindPoint(),
        m_pipeline->getPipelineLayout()->getHandle(),
        m_firstSet,
        m_setCount,
        m_sets.data() + m_firstSet, // NOLINT
        m_dynamicOffsetCount,
        m_dynamicOffsets.data() + m_firstDynamicOffset); // NOLINT
}

void Material::bind(const VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets) {
    vkCmdBindDescriptorSets(
        cmdBuffer,
        m_pipeline->getBindPoint(),
        m_pipeline->getPipelineLayout()->getHandle(),
        m_firstSet,
        m_setCount,
        m_sets.data() + m_firstSet, // NOLINT
        m_dynamicOffsetCount,
        dynamicBufferOffsets.data() + m_firstDynamicOffset); // NOLINT
}

} // namespace crisp
