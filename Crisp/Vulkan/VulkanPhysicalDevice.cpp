#include "Vulkan/VulkanPhysicalDevice.hpp"

#include <robin_hood/robin_hood.h>

namespace crisp
{
    VulkanPhysicalDevice::VulkanPhysicalDevice(VkPhysicalDevice handle)
        : m_handle(handle)
        , m_features({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 })
        , m_properties({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 })
        , m_rayTracingProperties({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV })
        , m_memoryProperties({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 })
        , m_features11({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES })
        , m_features12({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES })
        , m_properties11({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES })
        , m_properties12({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES })
    {
        m_features.pNext = &m_features11;
        m_features11.pNext = &m_features12;
        vkGetPhysicalDeviceFeatures2(m_handle, &m_features);

        m_features11.pNext = nullptr;

        m_properties.pNext = &m_rayTracingProperties;
        m_rayTracingProperties.pNext = &m_properties11;
        m_properties11.pNext = &m_properties12;
        vkGetPhysicalDeviceProperties2(m_handle, &m_properties);

        vkGetPhysicalDeviceMemoryProperties2(m_handle, &m_memoryProperties);
    }

    VulkanPhysicalDevice::~VulkanPhysicalDevice()
    {
    }

    bool VulkanPhysicalDevice::isSuitable(VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions) const
    {
        if (!queryQueueFamilyIndices(surface).isComplete())
            return false;

        if (!supportsDeviceExtensions(deviceExtensions))
            return false;

        if (getProperties().deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            return false;

        const SurfaceSupport surfaceSupport = querySurfaceSupport(surface);
        if (surfaceSupport.formats.empty() || surfaceSupport.presentModes.empty())
            return false;

        return true;
    }

    QueueFamilyIndices VulkanPhysicalDevice::queryQueueFamilyIndices(VkSurfaceKHR surface) const
    {
        QueueFamilyIndices indices;

        const auto queueFamilies = queryQueueFamilyProperties();

        for (int i = 0; i < queueFamilies.size(); ++i)
        {
            const auto& queueFamily = queueFamilies[i];

            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphicsFamily = i;

            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
                indices.computeFamily = i;

            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
                indices.transferFamily = i;

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_handle, i, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport)
                indices.presentFamily = i;

            if (indices.isComplete())
                return indices;
        }

        return indices;
    }

    SurfaceSupport VulkanPhysicalDevice::querySurfaceSupport(VkSurfaceKHR surface) const
    {
        SurfaceSupport surfaceSupport{ surface };
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_handle, surface, &surfaceSupport.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface, &formatCount, nullptr);
        if (formatCount > 0)
        {
            surfaceSupport.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface, &formatCount, surfaceSupport.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface, &presentModeCount, nullptr);
        if (presentModeCount > 0)
        {
            surfaceSupport.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface, &presentModeCount, surfaceSupport.presentModes.data());
        }

        return surfaceSupport;
    }

    std::vector<VkQueueFamilyProperties> VulkanPhysicalDevice::queryQueueFamilyProperties() const
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount, queueFamilies.data());

        return queueFamilies;
    }

    bool VulkanPhysicalDevice::supportsDeviceExtensions(const std::vector<const char*>& deviceExtensions) const
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &extensionCount, availableExtensions.data());

        robin_hood::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& ext : availableExtensions)
            requiredExtensions.erase(ext.extensionName);

        return requiredExtensions.empty();
    }
}