#pragma once

#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>
#include <CrispCore/Result.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <functional>

namespace crisp
{
    using SurfaceCreator = std::function<VkResult(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

    class VulkanContext
    {
    public:
        VulkanContext(SurfaceCreator surfaceCreator, std::vector<std::string>&& platformExtensions, bool enableValidationLayers);
        ~VulkanContext();

        VulkanContext(const VulkanContext& other) = delete;
        VulkanContext(VulkanContext&& other) noexcept;
        VulkanContext& operator=(const VulkanContext& other) = delete;
        VulkanContext& operator=(VulkanContext&& other) noexcept;

        Result<VulkanPhysicalDevice> selectPhysicalDevice(std::vector<std::string>&& deviceExtensions) const;

        VkSurfaceKHR getSurface() const;

    private:
        VkInstance               m_instance;
        VkDebugUtilsMessengerEXT m_debugMessenger;
        VkSurfaceKHR             m_surface;
    };
}
