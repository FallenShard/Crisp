#pragma once

#include <vector>
#include <functional>
#include <optional>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanQueueConfiguration;

    using SurfaceCreator = std::function<VkResult(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

    struct QueueFamilyIndices
    {
        int graphicsFamily = -1;
        int presentFamily  = -1;
        int computeFamily  = -1;
        int transferFamily = -1;

        bool isComplete()
        {
            return graphicsFamily >= 0 && presentFamily >= 0 && computeFamily >= 0 && transferFamily >= 0;
        }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class VulkanContext
    {
    public:
        VulkanContext(SurfaceCreator surfaceCreator, std::vector<std::string>&& reqPlatformExtensions);
        ~VulkanContext();

        VkPhysicalDevice getPhysicalDevice() const;
        VkSurfaceKHR getSurface() const;
        VkDevice createLogicalDevice(const VulkanQueueConfiguration& config) const;

        bool getQueueFamilyPresentationSupport(uint32_t familyIndex) const;
        std::vector<VkQueueFamilyProperties> getQueueFamilyProperties() const;
        QueueFamilyIndices findQueueFamilies() const;
        SwapChainSupportDetails querySwapChainSupport() const;

        std::optional<uint32_t> findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        std::optional<uint32_t> findMemoryType(VkMemoryPropertyFlags properties) const;
        std::optional<uint32_t> findDeviceImageMemoryType(VkDevice device);
        std::optional<uint32_t> findDeviceBufferMemoryType(VkDevice device);
        std::optional<uint32_t> findStagingBufferMemoryType(VkDevice device);

        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
        VkFormat findSupportedDepthFormat() const;

        const VkPhysicalDeviceProperties& getPhysicalDeviceProperties() const;
        const VkPhysicalDeviceFeatures&   getPhysicalDeviceFeatures() const;
        const VkPhysicalDeviceLimits&     getPhysicalDeviceLimits() const;

    private:
        VkInstance               m_instance;
        VkDebugReportCallbackEXT m_debugCallback;
        VkSurfaceKHR             m_surface;

        VkPhysicalDevice           m_physicalDevice; // Implicitly cleaned up with VkInstance
        VkPhysicalDeviceFeatures   m_physicalDeviceFeatures;
        VkPhysicalDeviceProperties m_physicalDeviceProperties;
    };
}
