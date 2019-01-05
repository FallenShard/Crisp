#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;

    template <typename T>
    class VulkanResource
    {
    public:
        VulkanResource(VulkanDevice* device) : m_device(device), m_handle(VK_NULL_HANDLE) {}
        VulkanResource(VulkanDevice* device, T handle) : m_device(device), m_handle(handle) {}
        virtual ~VulkanResource() {}

        inline VulkanDevice* getDevice() const { return m_device; }
        inline T getHandle() const { return m_handle; }
        inline operator T() const { return m_handle; }

    protected:
        VulkanDevice* m_device;
        T             m_handle;
    };
}