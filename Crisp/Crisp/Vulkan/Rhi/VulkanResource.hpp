#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Renderer/RendererConfig.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResourceDeallocator.hpp>

namespace crisp {
template <typename T>
class VulkanResource {
public:
    VulkanResource(const VulkanResource& other) = delete;
    VulkanResource& operator=(const VulkanResource& other) = delete;

    VulkanResource(VulkanResource&& other) noexcept
        : m_deallocator(std::exchange(other.m_deallocator, nullptr))
        , m_handle(std::exchange(other.m_handle, VK_NULL_HANDLE))
        , m_framesToLive(std::exchange(other.m_framesToLive, 0)) {}

    VulkanResource& operator=(VulkanResource&& other) noexcept {
        m_deallocator = std::exchange(other.m_deallocator, nullptr);
        m_handle = std::exchange(other.m_handle, VK_NULL_HANDLE);
        m_framesToLive = std::exchange(other.m_framesToLive, 0);
        return *this;
    }

    template <typename DestroyFunc>
    static void destroyVulkanHandle(void* handle, VulkanResourceDeallocator* deallocator, const DestroyFunc& func) {
        SPDLOG_DEBUG("Destroying object at address {}: {}.", handle, typeid(T).name());
        func(deallocator->getDeviceHandle(), static_cast<T>(handle), nullptr);
    }

    T getHandle() const {
        return m_handle;
    }

    void swap(VulkanResource& rhs) noexcept {
        std::swap(m_deallocator, rhs.m_deallocator);
        std::swap(m_handle, rhs.m_handle);
        std::swap(m_framesToLive, rhs.m_framesToLive);
    }

    void setDeferredDestruction(bool isEnabled) {
        m_framesToLive = isEnabled ? kRendererVirtualFrameCount : 0;
    }

protected:
    explicit VulkanResource(VulkanResourceDeallocator& deallocator)
        : m_handle(VK_NULL_HANDLE)
        , m_deallocator(&deallocator) {}

    VulkanResource(const T handle, VulkanResourceDeallocator& deallocator)
        : m_handle(handle)
        , m_deallocator(&deallocator) {}

    ~VulkanResource() {
        if (getDestroyFunc<T>() != nullptr) {
            if (!m_handle || !m_deallocator) {
                return;
            }

            if (m_framesToLive == 0) {
                destroyVulkanHandle(m_handle, m_deallocator, getDestroyFunc<T>());
            } else {
                m_deallocator->deferDestruction(
                    m_framesToLive, m_handle, [](void* handle, VulkanResourceDeallocator* deallocator) {
                        destroyVulkanHandle(handle, deallocator, getDestroyFunc<T>());
                    });
            }
        } else {
            CRISP_FATAL("Didn't destroy object of type: {}", typeid(T).name());
        }
    }

    T m_handle;
    VulkanResourceDeallocator* m_deallocator;
    int32_t m_framesToLive = kRendererVirtualFrameCount;
};
} // namespace crisp
