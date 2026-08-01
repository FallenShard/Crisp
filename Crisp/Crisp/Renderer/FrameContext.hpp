#pragma once

#include <cstdint>

#include <Crisp/Vulkan/Rhi/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>
#include <Crisp/Vulkan/VulkanStagingBelt.hpp>

namespace crisp {

struct FrameContext {
    uint64_t frameIndex;
    uint32_t virtualFrameIndex;
    uint32_t swapChainImageIndex;
    VulkanCommandBuffer* commandBuffer;
    VulkanCommandEncoder commandEncoder;
    VulkanStagingBelt* stagingBelt;

    // Timeline value this frame's submission signals. Stamp anything recorded now that must outlive it.
    uint64_t completionValue;

    // Highest timeline value the GPU has finished, sampled at the start of this frame.
    uint64_t completedValue;
};
} // namespace crisp
