#pragma once

#include <Crisp/Renderer/StorageBuffer.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/VulkanRingBuffer.hpp>
#include <Crisp/Vulkan/DescriptorSetAllocator.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <vector>

namespace crisp {
class Material {
public:
    explicit Material(VulkanPipeline* pipeline);
    Material(VulkanPipeline* pipeline, DescriptorSetAllocator* descriptorSetAllocator);

    // Methods to update a descriptor referencing an image.
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const VkDescriptorImageInfo& imageInfo);
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
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const UniformBuffer& uniformBuffer);
    void writeDescriptor(
        uint32_t setIndex,
        uint32_t binding,
        const UniformBuffer& uniformBuffer,
        uint32_t elementSize,
        uint32_t elementCount);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const StorageBuffer& storageBuffer);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanRingBuffer& buffer);

    void writeDescriptor(
        uint32_t setIndex, uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR& asInfo);

    void setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset);
    void setDynamicBufferView(uint32_t index, const UniformBuffer& dynamicUniformBuffer, uint32_t offset);

    void bind(
        uint32_t frameIdx, VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
    void bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets);

    inline VulkanPipeline* getPipeline() const {
        return m_pipeline;
    }

    inline void setPipeline(VulkanPipeline* pipeline) {
        m_pipeline = pipeline;
    }

    inline const std::vector<DynamicBufferView>& getDynamicBufferViews() const {
        return m_dynamicBufferViews;
    }

    VkDescriptorSet getDescriptorSet(const uint32_t setIndex, const uint32_t frameIndex) const {
        return m_sets.at(frameIndex).at(setIndex);
    }

private:
    std::array<std::vector<VkDescriptorSet>, RendererConfig::VirtualFrameCount> m_sets;
    std::array<std::vector<uint32_t>, RendererConfig::VirtualFrameCount> m_dynamicOffsets;
    std::vector<DynamicBufferView> m_dynamicBufferViews;

    VulkanDevice* m_device;
    VulkanPipeline* m_pipeline;
};
} // namespace crisp