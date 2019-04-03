#include "VulkanBufferUtils.hpp"

namespace crisp
{
    std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice* device, const void* data, VkDeviceSize size)
    {
        auto stagingBuffer = std::make_unique<VulkanBuffer>(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        stagingBuffer->updateFromHost(data, size, 0);
        return stagingBuffer;
    }
}