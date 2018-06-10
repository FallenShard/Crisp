#pragma once

#include <vector>
#include <list>

#include <vulkan/vulkan.h>

#include "Renderer/Renderer.hpp"

namespace crisp
{
    class VulkanPipeline;
    class VulkanDevice;

    class Material
    {
    public:
        Material(VulkanPipeline* pipeline, std::vector<uint32_t> sharedSets, std::vector<uint32_t> uniqueSets = std::vector<uint32_t>());

        VkWriteDescriptorSet makeDescriptorWrite(uint32_t setIdx, uint32_t binding, uint32_t frameIdx = 0);

        void setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset);
        void bind(uint32_t frameIdx, VkCommandBuffer commandBuffer);

    private:
        std::array<std::vector<VkDescriptorSet>,  Renderer::NumVirtualFrames>  m_sets;
        mutable std::array<std::vector<uint32_t>, Renderer::NumVirtualFrames>  m_dynamicOffsets;
        VulkanPipeline* m_pipeline;
    };
}