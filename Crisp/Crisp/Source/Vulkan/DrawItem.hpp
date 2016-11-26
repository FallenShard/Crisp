#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace crisp
{
    struct DrawItem
    {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        uint32_t descriptorSetOffset;
        std::vector<VkDescriptorSet> descriptorSets;

        uint32_t vertexBufferBindingOffset;
        std::vector<VkDeviceSize> vertexBufferOffsets;
        std::vector<VkBuffer> vertexBuffers;

        VkBuffer indexBuffer;
        VkIndexType indexType;
        VkDeviceSize indexBufferOffset;

        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        uint32_t vertexOffset;
        uint32_t firstInstance;
    };
}