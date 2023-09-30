#pragma once

#include <Crisp/Core/Result.hpp>
#include <Crisp/Vulkan/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>

#include <functional>
#include <vector>

namespace crisp {
using SurfaceCreator = std::function<VkResult(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

class VulkanContext {
public:
    VulkanContext(
        SurfaceCreator&& surfaceCreator, std::vector<std::string>&& platformExtensions, bool enableValidationLayers);
    ~VulkanContext();

    VulkanContext(const VulkanContext& other) = delete;
    VulkanContext(VulkanContext&& other) = delete;
    VulkanContext& operator=(const VulkanContext& other) = delete;
    VulkanContext& operator=(VulkanContext&& other) = delete;

    Result<VulkanPhysicalDevice> selectPhysicalDevice(std::vector<std::string>&& deviceExtensions) const;

    VkSurfaceKHR getSurface() const;

    VkInstance getInstance() const;

private:
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkSurfaceKHR m_surface;
};
} // namespace crisp
