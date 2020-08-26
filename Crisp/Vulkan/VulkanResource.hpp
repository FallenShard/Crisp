#pragma once

#include <vulkan/vulkan.h>

#include <CrispCore/Log.hpp>

#include "VulkanDevice.hpp"

#include <string>

namespace crisp
{
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
            , m_deferDestruction(std::move(other.m_deferDestruction))
        {
            other.m_device = nullptr;
            other.m_handle = VK_NULL_HANDLE;
            other.m_deferDestruction = true;
        }

        virtual ~VulkanResource() = 0 {};

        VulkanResource& operator=(const VulkanResource& other) = delete;
        VulkanResource& operator=(VulkanResource&& other) noexcept
        {
            if (this != &other)
            {
                m_device = std::move(other.m_device);
                m_handle = std::move(other.m_handle);
                m_deferDestruction = std::move(other.m_deferDestruction);
                other.m_device = nullptr;
                other.m_handle = VK_NULL_HANDLE;
                other.m_deferDestruction = true;
            }

            return *this;
        }

        inline VulkanDevice* getDevice() const { return m_device; }
        inline T             getHandle() const { return m_handle; }

        inline void swap(VulkanResource& rhs)
        {
            std::swap(m_device, rhs.m_device);
            std::swap(m_handle, rhs.m_handle);
            std::swap(m_deferDestruction, rhs.m_deferDestruction);
        }

        inline void setDeferredDestruction(bool isEnabled) { m_deferDestruction = isEnabled; }

        inline void setTag(const std::string& tag) { m_tag = tag; }

    protected:
        VulkanDevice* m_device;
        T             m_handle;
        bool m_deferDestruction = true;

        std::string m_tag;
    };
}
