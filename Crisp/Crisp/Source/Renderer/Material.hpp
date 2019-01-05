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
        Material(VulkanPipeline* pipeline, std::vector<uint32_t> singleSets, std::vector<uint32_t> bufferedSets = std::vector<uint32_t>());

        VkWriteDescriptorSet makeDescriptorWrite(uint32_t setIdx, uint32_t binding, uint32_t frameIdx = 0);
        VkWriteDescriptorSet makeDescriptorWrite(uint32_t setIdx, uint32_t binding, uint32_t index, uint32_t frameIdx = 0);

        void writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, VkDescriptorBufferInfo&& bufferInfo);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, VkDescriptorImageInfo&& imageInfo);
        void writeDescriptor(uint32_t setIndex, uint32_t binding, uint32_t frameIdx, const VulkanImageView& imageView, VkSampler sampler = VK_NULL_HANDLE);
        void writeDescriptor(uint32_t setIdx, uint32_t binding, uint32_t arrayIndex, uint32_t frameIdx = 0);

        void setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset);
        void bind(uint32_t frameIdx, VkCommandBuffer commandBuffer);

        inline VulkanPipeline* getPipeline() const { return m_pipeline; }

        void addDynamicBufferInfo(const UniformBuffer& dynamicUniformBuffer, uint32_t offset);
        inline const std::vector<DynamicBufferInfo>& getDynamicBufferInfos() const { return m_dynamicBufferInfos; }

    private:
        std::array<std::vector<VkDescriptorSet>,  Renderer::NumVirtualFrames>  m_sets;
        mutable std::array<std::vector<uint32_t>, Renderer::NumVirtualFrames>  m_dynamicOffsets;
        std::vector<DynamicBufferInfo> m_dynamicBufferInfos;

        VulkanPipeline* m_pipeline;
    };
}