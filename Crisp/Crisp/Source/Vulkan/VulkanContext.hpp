#pragma once

#include <vector>
#include <functional>

#include <vulkan/vulkan.h>

namespace crisp
{
    using SurfaceCreator = std::function<VkResult(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

    struct QueueFamilyIndices
    {
        int graphicsFamily = -1;
        int presentFamily = -1;

        bool isComplete()
        {
            return graphicsFamily >= 0 && presentFamily >= 0;
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
        VulkanContext(SurfaceCreator surfaceCreator, std::vector<const char*> reqPlatformExtensions);
        ~VulkanContext();

        VkPhysicalDevice getPhysicalDevice() const;
        VkSurfaceKHR getSurface() const;
        VkDevice createLogicalDevice();

        QueueFamilyIndices findQueueFamilies() const;
        bool checkDeviceExtensionSupport() const;
        SwapChainSupportDetails querySwapChainSupport() const;
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        uint32_t findMemoryType(VkMemoryPropertyFlags properties) const;
        uint32_t findDeviceImageMemoryType(VkDevice device);
        uint32_t findDeviceBufferMemoryType(VkDevice device);
        uint32_t findStagingBufferMemoryType(VkDevice device);

        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
        VkFormat findSupportedDepthFormat() const;

        VkPhysicalDeviceProperties getDeviceProperties() const;

    private:
        void createInstance(std::vector<const char*> reqPlatformExtensions);
        void setupDebugCallback();
        void createSurface(SurfaceCreator surfaceCreator);
        void pickPhysicalDevice();

        bool checkRequiredExtensions(std::vector<const char*> reqExtensions, const std::vector<VkExtensionProperties>& supportedExtensions);
        bool checkValidationLayerSupport();

        bool isDeviceSuitable(VkPhysicalDevice device);

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
        bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

        const static std::vector<const char*> validationLayers;
        const static std::vector<const char*> deviceExtensions;

        VkInstance m_instance;
        VkDebugReportCallbackEXT m_callback;
        VkSurfaceKHR m_surface;

        VkPhysicalDevice m_physicalDevice; // Implicitly cleaned up with VkInstance
    };
}
