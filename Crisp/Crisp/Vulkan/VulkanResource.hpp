#pragma once

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Renderer/RendererConfig.hpp>
#include <Crisp/Vulkan/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanResourceDeallocator.hpp>

namespace crisp
{
template <typename T>
class VulkanResource
{
public:
    VulkanResource(VulkanResourceDeallocator& deallocator)
        : m_handle(VK_NULL_HANDLE)
        , m_deallocator(&deallocator)
    {
    }

    VulkanResource(T handle, VulkanResourceDeallocator& deallocator)
        : m_handle(handle)
        , m_deallocator(&deallocator)
    {
    }

    VulkanResource(const VulkanResource& other) = delete;

    VulkanResource(VulkanResource&& other) noexcept
        : m_deallocator(std::exchange(other.m_deallocator, nullptr))
        , m_handle(std::exchange(other.m_handle, VK_NULL_HANDLE))
        , m_framesToLive(std::exchange(other.m_framesToLive, 0))
    {
    }

    virtual ~VulkanResource()
    {
        if (getDestroyFunc<T>() != nullptr)
        {
            if (!m_handle || !m_deallocator)
            {
                return;
            }

            m_deallocator->deferDestruction(
                m_framesToLive,
                m_handle,
                [](void* handle, VulkanResourceDeallocator* deallocator)
                {
                    destroyDeferred(handle, deallocator, getDestroyFunc<T>());
                });
        }
        else
        {
            spdlog::error("Didn't destroy object of type: {}", typeid(T).name());
        }
    }

    template <typename DestroyFunc>
    static void destroyDeferred(void* handle, VulkanResourceDeallocator* deallocator, DestroyFunc&& func)
    {
        spdlog::debug("Destroying object {} at address {}: {}.", deallocator->getTag(handle), handle, typeid(T).name());
        func(deallocator->getDeviceHandle(), static_cast<T>(handle), nullptr);
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

    inline T getHandle() const
    {
        return m_handle;
    }

    inline void swap(VulkanResource& rhs)
    {
        std::swap(m_deallocator, rhs.m_deallocator);
        std::swap(m_handle, rhs.m_handle);
        std::swap(m_framesToLive, rhs.m_framesToLive);
    }

    inline void setDeferredDestruction(bool isEnabled)
    {
        m_framesToLive = isEnabled ? RendererConfig::VirtualFrameCount : 1;
    }

    inline void setTag(std::string tag) const
    {
        m_deallocator->setTag(m_handle, std::move(tag));
    }

protected:
    T m_handle;
    VulkanResourceDeallocator* m_deallocator;
    int32_t m_framesToLive = RendererConfig::VirtualFrameCount;
};
} // namespace crisp
