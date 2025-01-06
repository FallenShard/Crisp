#pragma once

#include <cstdint>

#include <Crisp/Vulkan/Rhi/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

namespace crisp {
struct FrameContext {
    uint64_t frameIndex;
    uint32_t virtualFrameIndex;
    uint32_t swapChainImageIndex;
    VulkanCommandBuffer* commandBuffer;
    VulkanCommandEncoder commandEncoder;
};
} // namespace crisp
