#include <Crisp/Renderer/Material.hpp>

#include <Crisp/Core/Format.hpp>

namespace crisp {
namespace {
uint32_t findFirstOwnedDescriptorSet(const VulkanPipeline& pipeline) {
    const auto& layout = *pipeline.getPipelineLayout();
    uint32_t firstSet = 0;
    while (firstSet < layout.getDescriptorSetLayoutCount() && layout.isDescriptorSetLayoutExternal(firstSet)) {
        ++firstSet;
    }
    return firstSet;
}
} // namespace

Material::Material(VulkanPipeline* pipeline)
    : Material(
          pipeline,
          pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(),
          findFirstOwnedDescriptorSet(*pipeline),
          pipeline->getPipelineLayout()->getDescriptorSetLayoutCount() - findFirstOwnedDescriptorSet(*pipeline)) {}

Material::Material(VulkanPipeline* pipeline, VulkanDescriptorSetAllocator* descriptorSetAllocator)
    : Material(
          pipeline,
          descriptorSetAllocator,
          findFirstOwnedDescriptorSet(*pipeline),
          pipeline->getPipelineLayout()->getDescriptorSetLayoutCount() - findFirstOwnedDescriptorSet(*pipeline)) {}

Material::Material(VulkanPipeline* pipeline, const uint32_t firstSet, const uint32_t setCount)
    : Material(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), firstSet, setCount) {}

Material::Material(
    VulkanPipeline* pipeline,
    VulkanDescriptorSetAllocator* descriptorSetAllocator,
    const uint32_t firstSet,
    const uint32_t setCount)
    : m_sets(pipeline->getPipelineLayout()->getDescriptorSetLayoutCount(), VK_NULL_HANDLE)
    , m_firstSet(firstSet)
    , m_setCount(setCount)
    , m_device(const_cast<VulkanDevice*>(&descriptorSetAllocator->getDevice())) // NOLINT
    , m_pipeline(pipeline) {
    const auto& pipelineLayout{*m_pipeline->getPipelineLayout()};
    CRISP_CHECK_LE(m_firstSet + m_setCount, pipelineLayout.getDescriptorSetLayoutCount());

    for (uint32_t setIdx = m_firstSet; setIdx < m_firstSet + m_setCount; ++setIdx) {
        CRISP_CHECK(
            !pipelineLayout.isDescriptorSetLayoutExternal(setIdx),
            "Material cannot allocate externally owned descriptor set {}.",
            setIdx);
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

void Material::setDebugName(const std::string_view name) const {
    for (uint32_t setIndex = m_firstSet; setIndex < m_firstSet + m_setCount; ++setIndex) {
        m_device->setObjectName(m_sets[setIndex], fmt::format("{} Descriptor Set {}", name, setIndex));
    }
}

VkDescriptorSet Material::getOwnedDescriptorSet(const uint32_t setIndex) const {
    CRISP_CHECK_GE_LT(setIndex, m_firstSet, m_firstSet + m_setCount);
    CRISP_CHECK_NE(m_sets[setIndex], VK_NULL_HANDLE);
    return m_sets[setIndex];
}

void Material::writeDescriptor(const uint32_t setIndex, const uint32_t binding, const VkDescriptorImageInfo& imageInfo) {
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = getOwnedDescriptorSet(setIndex),
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
        },
        imageInfo);
}

void Material::writeDescriptor(
    const uint32_t setIndex, VkWriteDescriptorSet write, const VkDescriptorImageInfo& imageInfo) {
    write.dstSet = getOwnedDescriptorSet(setIndex);
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

void Material::writeBindlessDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const uint32_t arrayIndex,
    const VulkanImageView& imageView,
    const VulkanSampler* sampler) {
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = getOwnedDescriptorSet(setIndex),
            .dstBinding = binding,
            .dstArrayElement = arrayIndex,
            .descriptorCount = 1,
            .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
        },
        imageView.getDescriptorInfo(sampler));
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VkDescriptorBufferInfo& bufferInfo) {
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = getOwnedDescriptorSet(setIndex),
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = m_pipeline->getDescriptorType(setIndex, binding),
        },
        bufferInfo);
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, const uint32_t dstElement) {
    m_device->postDescriptorWrite(
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = getOwnedDescriptorSet(setIndex),
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
    m_device->postDescriptorWrite({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asInfo,
        .dstSet = getOwnedDescriptorSet(setIndex),
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

VulkanDescriptorSetBinding Material::getDescriptorSetBinding() const {
    return getDescriptorSetBinding(m_dynamicOffsets);
}

VulkanDescriptorSetBinding Material::getDescriptorSetBinding(
    const std::span<const uint32_t> dynamicBufferOffsets) const {
    CRISP_CHECK_LE(m_dynamicOffsetCount, dynamicBufferOffsets.size());
    return {
        .bindPoint = m_pipeline->getBindPoint(),
        .pipelineLayout = m_pipeline->getPipelineLayout()->getHandle(),
        .firstSet = m_firstSet,
        .descriptorSets = std::span{m_sets}.subspan(m_firstSet, m_setCount),
        .dynamicOffsets = dynamicBufferOffsets.first(m_dynamicOffsetCount),
    };
}

} // namespace crisp
