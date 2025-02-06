#pragma once

#include <Crisp/Vulkan/Rhi/VulkanDescriptorSetAllocator.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
class Material {
public:
    explicit Material(VulkanPipeline* pipeline);
    Material(VulkanPipeline* pipeline, uint32_t firstSet, uint32_t setCount);
    Material(VulkanPipeline* pipeline, VulkanDescriptorSetAllocator* VulkanDescriptorSetAllocator);
    Material(
        VulkanPipeline* pipeline,
        VulkanDescriptorSetAllocator* VulkanDescriptorSetAllocator,
        uint32_t firstSet,
        uint32_t setCount);

    // Methods to update a descriptor referencing an image.
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const VkDescriptorImageInfo& imageInfo);
    void writeDescriptor(uint32_t setIndex, VkWriteDescriptorSet write, const VkDescriptorImageInfo& imageInfo);
    void writeDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        const VulkanImageView& imageView,
        const VulkanSampler& sampler,
        std::optional<VkImageLayout> imageLayout = std::nullopt);
    void writeDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        const VulkanRenderPass& renderPass,
        uint32_t renderTargetIndex,
        const VulkanSampler* sampler);
    void writeBindlessDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        uint32_t arrayIndex,
        const VulkanImageView& imageView,
        const VulkanSampler* sampler);

    // Methods to update a descriptor referencing a buffer.
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const VkDescriptorBufferInfo& bufferInfo);
    void writeDescriptor(
        uint32_t setIndex, uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, uint32_t dstElement);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanBuffer& buffer);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanRingBuffer& buffer);

    void writeDescriptor(
        uint32_t setIndex, uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR& asInfo);

    void setDynamicOffset(uint32_t index, uint32_t offset);

    void bind(VkCommandBuffer cmdBuffer);
    void bind(VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets);

    VulkanPipeline* getPipeline() const {
        return m_pipeline;
    }

    VkDescriptorSet getDescriptorSet(const uint32_t setIndex) const {
        CRISP_CHECK_GE_LT(setIndex, 0, m_sets.size());
        return m_sets[setIndex];
    }

    void setBindRange(uint32_t firstSet, uint32_t setCount, uint32_t firstDynamicOffset, uint32_t dynamicOffsetCount) {
        m_firstSet = firstSet;
        m_setCount = setCount;
        m_firstDynamicOffset = firstDynamicOffset;
        m_dynamicOffsetCount = dynamicOffsetCount;
    }

    uint32_t getDynamicDescriptorCount() const {
        return static_cast<uint32_t>(m_dynamicOffsets.size());
    }

private:
    std::vector<VkDescriptorSet> m_sets;
    std::vector<uint32_t> m_dynamicOffsets;

    uint32_t m_firstSet{0};
    uint32_t m_setCount{0};
    uint32_t m_firstDynamicOffset{0};
    uint32_t m_dynamicOffsetCount{0};

    VulkanDevice* m_device;
    VulkanPipeline* m_pipeline;
};
} // namespace crisp