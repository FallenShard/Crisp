#pragma once

#include "Vulkan/VulkanBuffer.hpp"
#include <memory>

namespace crisp
{
    std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice* device, const void* data, VkDeviceSize size);
}