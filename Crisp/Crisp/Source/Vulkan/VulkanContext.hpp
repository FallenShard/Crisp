#pragma once

#include <vector>
#include <functional>

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
        void createInstance(std::vector<std::string>&& reqPlatformExtensions);
        void setupDebugCallback();
        void createSurface(SurfaceCreator surfaceCreator);
        void pickPhysicalDevice();

        bool checkRequiredExtensions(std::vector<const char*> reqExtensions, const std::vector<VkExtensionProperties>& supportedExtensions);
        bool checkValidationLayerSupport();

        bool isDeviceSuitable(VkPhysicalDevice device);

        std::vector<VkQueueFamilyProperties> getQueueFamilyProperties(VkPhysicalDevice device) const;
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
        bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

        const static std::vector<const char*> validationLayers;
        const static std::vector<const char*> deviceExtensions;

        VkInstance               m_instance;
        VkDebugReportCallbackEXT m_callback;
        VkSurfaceKHR             m_surface;
        VkPhysicalDevice         m_physicalDevice; // Implicitly cleaned up with VkInstance
    };
}
