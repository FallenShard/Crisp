#pragma once

#include <Crisp/Renderer/RendererConfig.hpp>
#include <Crisp/Vulkan/VulkanResourceDeallocator.hpp>

#include <CrispCore/Logger.hpp>

#include <vulkan/vulkan.h>

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
        VulkanResource(VulkanResourceDeallocator& deallocator) : m_handle(VK_NULL_HANDLE), m_deallocator(&deallocator) {}
        VulkanResource(T handle, VulkanResourceDeallocator& deallocator) : m_handle(handle), m_deallocator(&deallocator) {}

        VulkanResource(const VulkanResource& other) = delete;
        VulkanResource(VulkanResource&& other) noexcept
            : m_deallocator(std::exchange(other.m_deallocator, nullptr))
            , m_handle(std::exchange(other.m_handle, VK_NULL_HANDLE))
            , m_framesToLive(std::exchange(other.m_framesToLive, 0))
        {
        }

        virtual ~VulkanResource()
        {
            if constexpr (destroyFunc != nullptr)
            {
                if (!m_handle || !m_deallocator)
                    return;

                m_deallocator->deferDestruction(m_framesToLive, m_handle, destroyDeferred);
            }
        }

        static void destroyDeferred(void* handle, VulkanResourceDeallocator* deallocator)
        {
            spdlog::debug("Destroying object {}: {}.", deallocator->getTag(handle), typeid(T).name());
            destroyFunc(deallocator->getDeviceHandle(), static_cast<T>(handle), nullptr);
            deallocator->removeTag(handle);
        }

        VulkanResource& operator=(const VulkanResource& other) = delete;
        VulkanResource& operator=(VulkanResource&& other) noexcept
        {
            m_deallocator = std::exchange(other.m_deallocator, nullptr);
            m_handle = std::exchange(other.m_handle, VK_NULL_HANDLE);
            m_framesToLive = std::exchange(other.m_framesToLive, 0);
            return *this;
        }

        inline T getHandle() const { return m_handle; }

        inline void swap(VulkanResource& rhs)
        {
            std::swap(m_deallocator, rhs.m_deallocator);
            std::swap(m_handle, rhs.m_handle);
            std::swap(m_framesToLive, rhs.m_framesToLive);
        }

        inline void setDeferredDestruction(bool isEnabled) { m_framesToLive = isEnabled ? RendererConfig::VirtualFrameCount : 1; }

        inline void setTag(std::string tag) const { m_deallocator->setTag(m_handle, std::move(tag)); }

    protected:
        T m_handle;
        VulkanResourceDeallocator* m_deallocator;
        int32_t m_framesToLive = RendererConfig::VirtualFrameCount;
    };
}
