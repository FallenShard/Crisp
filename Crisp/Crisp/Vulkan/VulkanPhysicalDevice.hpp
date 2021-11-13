#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace crisp
{
    struct QueueFamilyIndices
    {
        int graphicsFamily = -1;
        int presentFamily = -1;
        int computeFamily = -1;
        int transferFamily = -1;

        bool isComplete() const
        {
            return graphicsFamily >= 0 && presentFamily >= 0 && computeFamily >= 0 && transferFamily >= 0;
        }
    };

    struct SurfaceSupport
    {
        VkSurfaceKHR surface;
        VkSurfaceCapabilitiesKHR capabilities = {};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class VulkanPhysicalDevice
    {
    public:
        explicit VulkanPhysicalDevice(VkPhysicalDevice handle);

        VulkanPhysicalDevice(const VulkanPhysicalDevice& other) = delete;
        VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept = default;
        VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice& other) = delete;
        VulkanPhysicalDevice& operator=(VulkanPhysicalDevice&& other) noexcept = default;

        inline VkPhysicalDevice getHandle() const { return m_handle; }
        inline const VkPhysicalDeviceFeatures& getFeatures() const { return m_features.features; }
        inline const VkPhysicalDeviceFeatures2& getFeatures2() const { return m_features; }
        inline const VkPhysicalDeviceProperties& getProperties() const { return m_properties.properties; }
        inline const VkPhysicalDeviceLimits& getLimits() const { return m_properties.properties.limits; }
        inline const VkPhysicalDeviceRayTracingPropertiesNV& getRayTracingProperties() const { return m_rayTracingProperties; }
        inline const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const { return m_memoryProperties.memoryProperties; }

        bool isSuitable(VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions) const;

        QueueFamilyIndices queryQueueFamilyIndices(VkSurfaceKHR surface) const;
        SurfaceSupport querySurfaceSupport(VkSurfaceKHR surface) const;
        std::vector<VkQueueFamilyProperties> queryQueueFamilyProperties() const;

    private:
        bool supportsDeviceExtensions(const std::vector<const char*>& deviceExtensions) const;

        VkPhysicalDevice            m_handle; // Implicitly cleaned up with VkInstance
        VkPhysicalDeviceFeatures2   m_features;
        VkPhysicalDeviceProperties2 m_properties;
        VkPhysicalDeviceRayTracingPropertiesNV m_rayTracingProperties;

        VkPhysicalDeviceVulkan11Features m_features11;
        VkPhysicalDeviceVulkan12Features m_features12;

        VkPhysicalDeviceVulkan11Properties m_properties11;
        VkPhysicalDeviceVulkan12Properties m_properties12;

        VkPhysicalDeviceMemoryProperties2 m_memoryProperties;
    };
}
