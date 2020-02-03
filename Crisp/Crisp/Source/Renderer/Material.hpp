#pragma once

#include <vector>
#include <list>

#include <vulkan/vulkan.h>

#include "Renderer/Renderer.hpp"

#include "vulkan/VulkanImageView.hpp"
#include "Renderer/UniformBuffer.hpp"

namespace crisp
{
    class VulkanPipeline;
    class VulkanDevice;
    class UniformBuffer;

    class Material
    {
    public:
        Material(VulkanPipeline* pipeline);

        VkWriteDescriptorSet makeDescriptorWrite(uint32_t setIdx, uint32_t binding, uint32_t frameIdx = 0);
        VkWriteDescriptorSet makeDescriptorWrite(uint32_t setIdx, uint32_t binding, uint32_t index, uint32_t frameIdx = 0);

        void writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanRenderPass& renderPass, uint32_t renderTargetIndex, const VulkanSampler* sampler);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, const UniformBuffer& uniformBuffer);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, const UniformBuffer& uniformBuffer, int elementSize, int elementCount);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, VkDescriptorBufferInfo&& bufferInfo);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanImageView& imageView, const VulkanSampler* sampler);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, const VulkanImageView& imageView, const VulkanSampler* sampler, VkImageLayout imageLayout);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, const std::vector<std::unique_ptr<VulkanImageView>>& imageViews, const VulkanSampler* sampler, VkImageLayout imageLayout);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, VkDescriptorBufferInfo&& bufferInfo);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, VkDescriptorImageInfo&& imageInfo);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, const VulkanImageView& imageView, const VulkanSampler* sampler);

        void setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset);
        void bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
        void bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer, const std::vector<uint32_t>& dynamicBufferOffsets);

        inline VulkanPipeline* getPipeline() const { return m_pipeline; }

        void setDynamicBufferView(uint32_t index, const UniformBuffer& dynamicBuffer, uint32_t offset);
        inline const std::vector<DynamicBufferView>& getDynamicBufferViews() const { return m_dynamicBufferViews; }

    private:
        std::array<std::vector<VkDescriptorSet>,  Renderer::NumVirtualFrames>  m_sets;
        std::array<std::vector<uint32_t>, Renderer::NumVirtualFrames>  m_dynamicOffsets;
        std::vector<DynamicBufferView> m_dynamicBufferViews;

        VulkanPipeline* m_pipeline;
    };
}