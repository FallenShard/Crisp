#pragma once

#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>

namespace crisp
{
struct CommandStream
{
    VulkanCommandBuffer& commandBuffer;
    uint32_t frameInFlightIndex;
};
} // namespace crisp
