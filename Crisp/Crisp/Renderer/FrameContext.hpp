#pragma once

#include <cstdint>

#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>

namespace crisp {
struct FrameContext {
    uint64_t frameIndex;
    uint32_t virtualFrameIndex;
    uint32_t swapImageIndex;
    VulkanCommandBuffer* commandBuffer{nullptr};
};
} // namespace crisp
