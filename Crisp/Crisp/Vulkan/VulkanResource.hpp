#pragma once

#include <Crisp/Renderer/RendererConfig.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

#include <string>

namespace crisp
{
    namespace detail
    {
        template <typename T>
        using DestroyFunc = void(*)(VkDevice, T, const VkAllocationCallbacks*);
    }

    template <typename T, detail::DestroyFunc<T> destroyFunc>
    class VulkanResource
    {
    public:
        VulkanResource(VulkanDevice* device) : m_device(device), m_handle(VK_NULL_HANDLE) {}
        VulkanResource(VulkanDevice* device, T handle) : m_device(device), m_handle(handle) {}

        VulkanResource(const VulkanResource& other) = delete;
        VulkanResource(VulkanResource&& other) noexcept
            : m_device(std::move(other.m_device))
            , m_handle(std::move(other.m_handle))
            , m_framesToLive(std::move(other.m_framesToLive))
        {
            other.m_device = nullptr;
            other.m_handle = VK_NULL_HANDLE;
            other.m_framesToLive = true;
        }

        virtual ~VulkanResource()
        {
            if constexpr (destroyFunc != nullptr)
            {
                m_device->addTag(m_handle, m_tag);
                m_device->deferDestruction(m_framesToLive, m_handle, [](void* handle, VulkanDevice* device)
                {
                    spdlog::debug("Destroying object {}: {} at frame {}", device->getTag(handle), typeid(T).name(), device->getCurrentFrameIndex());
                    destroyFunc(device->getHandle(), static_cast<T>(handle), nullptr);
                });
            }
        }

        VulkanResource& operator=(const VulkanResource& other) = delete;
        VulkanResource& operator=(VulkanResource&& other) noexcept
        {
            if (this != &other)
            {
                m_device = std::move(other.m_device);
                m_handle = std::move(other.m_handle);
                m_framesToLive = std::move(other.m_framesToLive);
                other.m_device = nullptr;
                other.m_handle = VK_NULL_HANDLE;
                other.m_framesToLive = 0;
            }

            return *this;
        }

        inline VulkanDevice* getDevice() const { return m_device; }
        inline T             getHandle() const { return m_handle; }

        inline void swap(VulkanResource& rhs)
        {
            std::swap(m_device, rhs.m_device);
            std::swap(m_handle, rhs.m_handle);
            std::swap(m_framesToLive, rhs.m_framesToLive);
            std::swap(m_tag, rhs.m_tag);
        }

        inline void setDeferredDestruction(bool isEnabled) { m_framesToLive = isEnabled ? RendererConfig::VirtualFrameCount : 1; }

        inline void setTag(const std::string& tag) { m_tag = tag; }

    protected:
        VulkanDevice* m_device;
        T             m_handle;
        int32_t m_framesToLive = RendererConfig::VirtualFrameCount;

        std::string m_tag;
    };
}
