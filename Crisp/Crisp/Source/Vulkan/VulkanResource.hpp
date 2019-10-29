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
        VulkanResource(const VulkanResource& other) = delete;
        VulkanResource(VulkanResource&& other) noexcept
            : m_device(std::move(other.m_device))
            , m_handle(std::move(other.m_handle))
        {
            other.m_device = nullptr;
            other.m_handle = VK_NULL_HANDLE;
        }

        virtual ~VulkanResource() = 0 {}

        VulkanResource& operator=(const VulkanResource& other) = delete;
        VulkanResource& operator=(VulkanResource&& other) noexcept
        {
            if (this != &other)
            {
                m_device = std::move(other.m_device);
                m_handle = std::move(other.m_handle);
                other.m_device = nullptr;
                other.m_handle = VK_NULL_HANDLE;
            }

            return *this;
        }

        inline VulkanDevice* getDevice() const { return m_device; }
        inline T getHandle() const { return m_handle; }
        inline operator T() const { return m_handle; }

    protected:
        VulkanDevice* m_device;
        T             m_handle;
    };
}