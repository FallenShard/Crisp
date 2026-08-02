#pragma once

#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

struct VulkanRasterizationPassDescriptor {
    std::vector<VkFormat> colorAttachmentFormats;
    VkFormat depthAttachmentFormat{VK_FORMAT_UNDEFINED};
    VkFormat stencilAttachmentFormat{VK_FORMAT_UNDEFINED};
    VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_1_BIT};
    uint32_t viewMask{0};
};

} // namespace crisp
