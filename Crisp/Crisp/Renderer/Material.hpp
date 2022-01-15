#pragma once

#include <list>
#include <vector>

#include <vulkan/vulkan.h>

#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/vulkan/VulkanImageView.hpp>

namespace crisp
{
class VulkanPipeline;
class VulkanDevice;
class UniformBuffer;
class StorageBuffer;
class DescriptorSetAllocator;

class Material
{
public:
    Material(VulkanPipeline* pipeline);
    Material(VulkanPipeline* pipeline, DescriptorSetAllocator* descriptorSetAllocator);

    void writeDescriptor(uint32_t setIndex, uint32_t binding, VkDescriptorImageInfo&& imageInfo);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanImageView& imageView,
        const VulkanSampler* sampler);

    void writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanRenderPass& renderPass,
        uint32_t renderTargetIndex, const VulkanSampler* sampler);
    void writeDescriptor(uint32_t setIndex, uint32_t binding,
        const std::vector<std::unique_ptr<VulkanImageView>>& imageViews, const VulkanSampler* sampler,
        VkImageLayout imageLayout);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const std::vector<VulkanImageView*>& imageViews,
        const VulkanSampler* sampler, VkImageLayout imageLayout);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, const VulkanImageView& imageView,
        const VulkanSampler* sampler);

    void writeDescriptor(uint32_t setIndex, uint32_t binding, VkDescriptorBufferInfo&& bufferInfo);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const UniformBuffer& uniformBuffer);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const UniformBuffer& uniformBuffer, int elementSize,
        int elementCount);
    void writeDescriptor(uint32_t setIndex, uint32_t binding, const StorageBuffer& storageBuffer);

    void setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset);
    void bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
    void bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets);

    inline VulkanPipeline* getPipeline() const
    {
        return m_pipeline;
    }
    inline void setPipeline(VulkanPipeline* pipeline)
    {
        m_pipeline = pipeline;
    }

    void setDynamicBufferView(uint32_t index, const UniformBuffer& dynamicBuffer, uint32_t offset);
    inline const std::vector<DynamicBufferView>& getDynamicBufferViews() const
    {
        return m_dynamicBufferViews;
    }

private:
    std::array<std::vector<VkDescriptorSet>, RendererConfig::VirtualFrameCount> m_sets;
    std::array<std::vector<uint32_t>, RendererConfig::VirtualFrameCount> m_dynamicOffsets;
    std::vector<DynamicBufferView> m_dynamicBufferViews;

    VulkanDevice* m_device;
    VulkanPipeline* m_pipeline;
};
} // namespace crisp