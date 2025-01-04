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
        uint32_t setIndex, uint32_t binding, const VulkanImageView& imageView, const VulkanSampler& sampler);
    void writeDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        const VulkanRenderPass& renderPass,
        uint32_t renderTargetIndex,
        const VulkanSampler* sampler);
    void writeDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        const std::vector<std::unique_ptr<VulkanImageView>>& imageViews,
        const VulkanSampler* sampler,
        VkImageLayout imageLayout);
    void writeDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        const std::vector<VulkanImageView*>& imageViews,
        const VulkanSampler* sampler,
        VkImageLayout imageLayout);
    void writeDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        uint32_t frameIndex,
        const VulkanImageView& imageView,
        const VulkanSampler* sampler);
    void writeDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        uint32_t frameIndex,
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

    void setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset);
    void setDynamicBufferView(uint32_t index, const VulkanRingBuffer& dynamicUniformBuffer, uint32_t offset);

    void bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer);
    void bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets);

    inline VulkanPipeline* getPipeline() const {
        return m_pipeline;
    }

    inline const std::vector<DynamicBufferView>& getDynamicBufferViews() const {
        return m_dynamicBufferViews;
    }

    VkDescriptorSet getDescriptorSet(const uint32_t setIndex, const uint32_t frameIndex) const {
        return m_sets.at(frameIndex).at(setIndex);
    }

    void setBindRange(uint32_t firstSet, uint32_t setCount, uint32_t firstDynamicOffset, uint32_t dynamicOffsetCount) {
        m_firstSet = firstSet;
        m_setCount = setCount;
        m_firstDynamicOffset = firstDynamicOffset;
        m_dynamicOffsetCount = dynamicOffsetCount;
    }

    struct RenderPassBindingData {
        uint32_t setIndex{0};
        uint32_t bindingIndex{0};
        uint32_t frameIndex{0};
        uint32_t arrayIndex{0};
        std::string renderPass;
        uint32_t attachmentIndex{0};
        std::string sampler;
    };

    std::vector<RenderPassBindingData>& getRenderPassBindings() {
        return m_renderPassBindings;
    }

private:
    std::array<std::vector<VkDescriptorSet>, kRendererVirtualFrameCount> m_sets;
    std::array<std::vector<uint32_t>, kRendererVirtualFrameCount> m_dynamicOffsets;
    std::vector<DynamicBufferView> m_dynamicBufferViews;

    uint32_t m_firstSet{0};
    uint32_t m_setCount{0};
    uint32_t m_firstDynamicOffset{0};
    uint32_t m_dynamicOffsetCount{0};

    VulkanDevice* m_device;
    VulkanPipeline* m_pipeline;

    std::vector<RenderPassBindingData> m_renderPassBindings;
};
} // namespace crisp