#include <Crisp/Renderer/Material.hpp>

namespace crisp {
Material::Material(VulkanPipeline* pipeline)
    : Material(pipeline, pipeline->getPipelineLayout()->getDescriptorSetAllocator()) {}

Material::Material(VulkanPipeline* pipeline, DescriptorSetAllocator* descriptorSetAllocator)
    : m_device(const_cast<VulkanDevice*>(&descriptorSetAllocator->getDevice())) // NOLINT
    , m_pipeline(pipeline) {
    const auto& pipelineLayout{*m_pipeline->getPipelineLayout()};
    const std::size_t setCount = pipelineLayout.getDescriptorSetLayoutCount();

    for (auto& perFrameSets : m_sets) {
        perFrameSets.resize(setCount);
    }

    for (uint32_t setIdx = 0; setIdx < setCount; ++setIdx) {
        const VkDescriptorSetLayout setLayout = pipelineLayout.getDescriptorSetLayout(setIdx);

        if (pipelineLayout.isDescriptorSetBuffered(setIdx)) {
            for (auto& perFrameSets : m_sets) {
                perFrameSets[setIdx] =
                    descriptorSetAllocator->allocate(setLayout, pipelineLayout.getDescriptorSetLayoutBindings(setIdx));
            }
        } else {
            const VkDescriptorSet set = descriptorSetAllocator->allocate(
                setLayout, m_pipeline->getPipelineLayout()->getDescriptorSetLayoutBindings(setIdx));
            for (auto& perFrameSets : m_sets) {
                perFrameSets[setIdx] = set;
            }
        }
    }

    m_dynamicBufferViews.resize(pipelineLayout.getDynamicBufferCount());
    for (auto& perFrameOffsets : m_dynamicOffsets) {
        perFrameOffsets.resize(m_dynamicBufferViews.size());
    }
}

void Material::writeDescriptor(const uint32_t setIndex, const uint32_t binding, const VkDescriptorImageInfo& imageInfo) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i) {
        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = m_sets[i][setIndex],
             .dstBinding = binding,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
            imageInfo);
    }
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VulkanImageView& imageView, const VulkanSampler& sampler) {
    writeDescriptor(setIndex, binding, imageView.getDescriptorInfo(&sampler));
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const VulkanRenderPass& renderPass,
    const uint32_t renderTargetIndex,
    const VulkanSampler* sampler) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i) {
        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = m_sets[i][setIndex],
             .dstBinding = binding,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
            renderPass.getAttachmentView(renderTargetIndex, i).getDescriptorInfo(sampler));
    }
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const std::vector<std::unique_ptr<VulkanImageView>>& imageViews,
    const VulkanSampler* sampler,
    const VkImageLayout imageLayout) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i) {
        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = m_sets[i][setIndex],
             .dstBinding = binding,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
            imageViews[i]->getDescriptorInfo(sampler, imageLayout));
    }
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const std::vector<VulkanImageView*>& imageViews,
    const VulkanSampler* sampler,
    const VkImageLayout imageLayout) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t i = 0; i < setsToUpdate; ++i) {
        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = m_sets[i][setIndex],
             .dstBinding = binding,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
            imageViews[i]->getDescriptorInfo(sampler, imageLayout));
    }
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const uint32_t frameIndex,
    const VulkanImageView& imageView,
    const VulkanSampler* sampler) {
    m_device->postDescriptorWrite(
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = m_sets[frameIndex][setIndex],
         .dstBinding = binding,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
        imageView.getDescriptorInfo(sampler));
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const uint32_t frameIndex,
    const uint32_t arrayIndex,
    const VulkanImageView& imageView,
    const VulkanSampler* sampler) {
    m_device->postDescriptorWrite(
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = m_sets[frameIndex][setIndex],
         .dstBinding = binding,
         .dstArrayElement = arrayIndex,
         .descriptorCount = 1,
         .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
        imageView.getDescriptorInfo(sampler));
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VkDescriptorBufferInfo& bufferInfo) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t frameIndex = 0; frameIndex < setsToUpdate; ++frameIndex) {
        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = m_sets[frameIndex][setIndex],
             .dstBinding = binding,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
            bufferInfo);
    }
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, const uint32_t dstElement) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t frameIndex = 0; frameIndex < setsToUpdate; ++frameIndex) {
        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = m_sets[frameIndex][setIndex],
             .dstBinding = binding,
             .dstArrayElement = dstElement,
             .descriptorCount = 1,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
            bufferInfo);
    }
}

void Material::writeDescriptor(const uint32_t setIndex, const uint32_t binding, const UniformBuffer& uniformBuffer) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t frameIndex = 0; frameIndex < setsToUpdate; ++frameIndex) {
        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = m_sets[frameIndex][setIndex],
             .dstBinding = binding,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
            uniformBuffer.getDescriptorInfo());
    }

    if (auto bufferIndex = m_pipeline->getPipelineLayout()->getDynamicBufferIndex(setIndex, binding);
        bufferIndex != ~0u) {
        setDynamicBufferView(bufferIndex, uniformBuffer, 0);
    }
}

void Material::writeDescriptor(
    const uint32_t setIndex,
    const uint32_t binding,
    const UniformBuffer& uniformBuffer,
    const uint32_t elementSize,
    const uint32_t elementCount) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t frameIndex = 0; frameIndex < setsToUpdate; ++frameIndex) {

        std::vector<VkDescriptorBufferInfo> infos;
        infos.reserve(elementCount);
        for (uint32_t j = 0; j < elementCount; ++j) {
            infos.emplace_back(uniformBuffer.getDescriptorInfo(elementSize * j, elementSize));
        }

        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = m_sets[frameIndex][setIndex],
             .dstBinding = binding,
             .dstArrayElement = 0,
             .descriptorCount = elementCount,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)},
            std::move(infos));
    }

    if (auto bufferIndex = m_pipeline->getPipelineLayout()->getDynamicBufferIndex(setIndex, binding);
        bufferIndex != ~0u) {
        setDynamicBufferView(bufferIndex, uniformBuffer, 0);
    }
}

void Material::writeDescriptor(const uint32_t setIndex, const uint32_t binding, const StorageBuffer& storageBuffer) {
    writeDescriptor(setIndex, binding, storageBuffer.getDescriptorInfo());
}

void Material::writeDescriptor(const uint32_t setIndex, const uint32_t binding, const VulkanRingBuffer& buffer) {
    writeDescriptor(setIndex, binding, buffer.getDescriptorInfo());
}

void Material::writeDescriptor(
    const uint32_t setIndex, const uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR& asInfo) {
    const uint32_t setsToUpdate =
        m_pipeline->getPipelineLayout()->isDescriptorSetBuffered(setIndex) ? RendererConfig::VirtualFrameCount : 1;
    for (uint32_t frameIndex = 0; frameIndex < setsToUpdate; ++frameIndex) {
        m_device->postDescriptorWrite(
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = &asInfo,
             .dstSet = m_sets[frameIndex][setIndex],
             .dstBinding = binding,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = m_pipeline->getDescriptorType(setIndex, binding)});
    }
}

void Material::setDynamicOffset(const uint32_t frameIdx, const uint32_t index, const uint32_t offset) {
    m_dynamicOffsets[frameIdx][index] = offset;
}

void Material::setDynamicBufferView(
    const uint32_t index, const UniformBuffer& dynamicUniformBuffer, const uint32_t offset) {
    m_dynamicBufferViews.at(index) = {&dynamicUniformBuffer, offset};

    for (uint32_t frameIndex = 0; frameIndex < RendererConfig::VirtualFrameCount; ++frameIndex) {
        m_dynamicOffsets.at(frameIndex).at(index) = dynamicUniformBuffer.getDynamicOffset(frameIndex);
    }
}

void Material::bind(const uint32_t frameIdx, const VkCommandBuffer cmdBuffer, const VkPipelineBindPoint bindPoint) {
    if (!m_sets[frameIdx].empty()) {
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

void Material::bind(
    const uint32_t frameIdx, const VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets) {
    if (!m_sets[frameIdx].empty()) {
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

} // namespace crisp
