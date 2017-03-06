#include "VulkanDevice.hpp"
#include "DeviceBuffer.hpp"

namespace crisp
{
    DeviceBuffer::DeviceBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usage, const void* srcData)
        : m_device(device)
    {
        m_handle = m_device.createDeviceBuffer(size, usage, srcData);
    }

    DeviceBuffer::~DeviceBuffer()
    {
    }

    void DeviceBuffer::resize(VkDeviceSize newSize)
    {
    }
}