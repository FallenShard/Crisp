#pragma once

#include <functional>
#include <vector>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
using SurfaceCreator = std::function<VkResult(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

class VulkanInstance {
public:
    VulkanInstance(
        const SurfaceCreator& surfaceCreator, std::vector<std::string>&& platformExtensions, bool enableValidationLayers);
    ~VulkanInstance();

    VulkanInstance(const VulkanInstance& other) = delete;
    VulkanInstance(VulkanInstance&& other) = delete;
    VulkanInstance& operator=(const VulkanInstance& other) = delete;
    VulkanInstance& operator=(VulkanInstance&& other) = delete;

    VkInstance getHandle() const;
    VkSurfaceKHR getSurface() const;

    uint32_t getApiVersion() const;

private:
    VkInstance m_handle;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkSurfaceKHR m_surface;
    uint32_t m_apiVersion;
};
} // namespace crisp
