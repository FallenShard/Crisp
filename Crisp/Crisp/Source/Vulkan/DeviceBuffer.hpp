#pragma once

#include "VulkanDevice.hpp"

namespace crisp
{
    class DeviceBuffer
    {
    public:
        DeviceBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usage, const void* srcData = nullptr);
        ~DeviceBuffer();

        void resize(VkDeviceSize newSize);

    private:
        VulkanDevice& m_device;

        VkBuffer m_handle;
    };
}