#pragma once

#include <span>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

struct VulkanDescriptorSetBinding {
    VkPipelineBindPoint bindPoint{VK_PIPELINE_BIND_POINT_GRAPHICS};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    uint32_t firstSet{0};
    std::span<const VkDescriptorSet> descriptorSets;
    std::span<const uint32_t> dynamicOffsets;
};

} // namespace crisp
