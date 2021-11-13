#pragma once

#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <functional>
#include <optional>

namespace crisp
{
    class VulkanQueueConfiguration;

    using SurfaceCreator = std::function<VkResult(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

    class VulkanContext
    {
    public:
        VulkanContext(SurfaceCreator surfaceCreator, std::vector<std::string>&& reqPlatformExtensions, bool enableValidationLayers);
        ~VulkanContext();

        VkSurfaceKHR getSurface() const;
        VkDevice     createLogicalDevice(const VulkanQueueConfiguration& config) const;

        bool getQueueFamilyPresentationSupport(uint32_t familyIndex) const;
        QueueFamilyIndices queryQueueFamilies() const;
        SurfaceSupport querySurfaceSupport() const;

        std::optional<uint32_t> findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        std::optional<uint32_t> findMemoryType(VkMemoryPropertyFlags properties) const;
        std::optional<uint32_t> findDeviceImageMemoryType(VkDevice device) const;
        std::optional<uint32_t> findDeviceBufferMemoryType(VkDevice device) const;
        std::optional<uint32_t> findStagingBufferMemoryType(VkDevice device) const;

        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
        VkFormat findSupportedDepthFormat() const;

        inline const VulkanPhysicalDevice& getPhysicalDevice() const { return m_physicalDevice; }

    private:
        VkInstance               m_instance;
        VkDebugUtilsMessengerEXT m_debugMessenger;
        VkSurfaceKHR             m_surface;

        VulkanPhysicalDevice m_physicalDevice;
    };
}
