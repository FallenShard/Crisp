#include "VulkanContext.hpp"

#include <algorithm>
#include <set>
#include <iostream>
#include <sstream>

#include <CrispCore/Log.hpp>
#include "Core/Application.hpp"
#include "VulkanQueueConfiguration.hpp"

namespace crisp
{
    namespace detail
    {
#ifndef _DEBUG
        constexpr bool EnableValidationLayers = false;
#else
        constexpr bool EnableValidationLayers = true;
#endif

        const std::vector<const char*> ValidationLayers =
        {
            "VK_LAYER_LUNARG_standard_validation"
        };

        const std::vector<const char*> DeviceExtensions =
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback)
        {
            auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
            if (func != nullptr)
                return func(instance, pCreateInfo, pAllocator, pCallback);
            else
                return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
        {
            auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
            if (func != nullptr)
                func(instance, callback, pAllocator);
        }

        VkBool32 debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char*, const char* message, void*)
        {
            std::stringstream stream;
            stream << "\n=== Vulkan Validation Layer ===\n";
            stream << "Severity: ";
            if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)         stream << " | Info";
            if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)             stream << " | Warning";
            if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) stream << " | Performance";
            if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)               stream << " | Error";
            if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)               stream << " | Debug";

            stream << '\n';

            ConsoleColor color = flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ? ConsoleColor::LightRed :
                flags & VK_DEBUG_REPORT_WARNING_BIT_EXT || flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ? ConsoleColor::Yellow : ConsoleColor::LightCyan;

            stream << message << "\n\n";

            ConsoleColorizer colorizer(color);
            fmt::print(stream.str());
            return VK_FALSE;
        }

        bool checkRequiredExtensions(std::vector<const char*> reqExtensions, const std::vector<VkExtensionProperties>& supportedExtensions)
        {
            std::set<std::string> reqExtsSet;

            ConsoleColorizer colorizer(ConsoleColor::Yellow);
            std::cout << "Platform-required Vulkan extensions: \n";
            for (auto extensionName : reqExtensions)
            {
                std::cout << "\t" << extensionName << '\n';
                reqExtsSet.insert(std::string(extensionName));
            }

            colorizer.set(ConsoleColor::Green);
            std::cout << "Supported extensions: " << '\n';
            for (const auto& ext : supportedExtensions)
            {

                std::cout << "\t" << ext.extensionName << '\n';
                reqExtsSet.erase(ext.extensionName); // Will hold unsupported required extensions, if any
            }

            size_t numSupportedReqExts = reqExtensions.size() - reqExtsSet.size();

            colorizer.set(ConsoleColor::LightGreen);
            std::cout << numSupportedReqExts << "/" << reqExtensions.size() << " required extensions supported." << '\n';
            std::cout << supportedExtensions.size() << " extensions supported total." << '\n';

            return numSupportedReqExts == reqExtensions.size();
        }

        bool areValidationLayersSupported()
        {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            std::set<std::string> availableLayersSet;
            for (const auto& layer : availableLayers)
                availableLayersSet.insert(layer.layerName);

            for (const auto& validationLayer : ValidationLayers)
                if (availableLayersSet.find(validationLayer) == availableLayersSet.end())
                    return false;

            std::cout << "Validation layers are supported!\n";

            return true;
        }

        VkInstance createInstance(std::vector<std::string>&& reqPlatformExtensions)
        {
            VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
            appInfo.pApplicationName   = Application::Title;
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName        = "CrispEngine";
            appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion         = VK_API_VERSION_1_1;

            if (EnableValidationLayers)
                reqPlatformExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

            std::vector<const char*> enabledExtensions;
            std::transform(reqPlatformExtensions.begin(), reqPlatformExtensions.end(), std::back_inserter(enabledExtensions), [](const auto& ext) { return ext.c_str(); });

            VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
            createInfo.pApplicationInfo        = &appInfo;
            createInfo.enabledExtensionCount   = static_cast<uint32_t>(enabledExtensions.size());
            createInfo.ppEnabledExtensionNames = enabledExtensions.data();
            createInfo.enabledLayerCount       = EnableValidationLayers ? static_cast<uint32_t>(ValidationLayers.size()) : 0;
            createInfo.ppEnabledLayerNames     = EnableValidationLayers ? ValidationLayers.data() : nullptr;

            VkInstance instance;
            vkCreateInstance(&createInfo, nullptr, &instance);

            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> extensionProps(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProps.data());

            bool allExtensionsSupported    = checkRequiredExtensions(enabledExtensions, extensionProps);
            bool validationLayersSupported = !EnableValidationLayers || areValidationLayersSupported();

            if (!allExtensionsSupported)
                throw std::runtime_error("Required extensions are not supported!");
            if (!validationLayersSupported)
                throw std::runtime_error("Validation layers requested are not available!");

            return instance;
        }

        VkDebugReportCallbackEXT createDebugCallback(VkInstance instance)
        {
            if (!EnableValidationLayers) return nullptr;

            VkDebugReportCallbackCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
            createInfo.flags       = VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
            createInfo.pfnCallback = reinterpret_cast<PFN_vkDebugReportCallbackEXT>(debugCallback);

            VkDebugReportCallbackEXT callback;
            CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback);
            return callback;
        }

        VkSurfaceKHR createSurface(VkInstance instance, SurfaceCreator surfaceCreator)
        {
            VkSurfaceKHR surface;
            surfaceCreator(instance, nullptr, &surface);
            return surface;
        }

        std::vector<VkQueueFamilyProperties> getQueueFamilyProperties(VkPhysicalDevice device)
        {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            return queueFamilies;
        }

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            QueueFamilyIndices indices;

            auto queueFamilies = getQueueFamilyProperties(device);

            int i = 0;
            for (const auto& queueFamily : queueFamilies)
            {
                if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    indices.graphicsFamily = i;

                if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
                    indices.computeFamily = i;

                if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
                    indices.transferFamily = i;

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
                if (queueFamily.queueCount > 0 && presentSupport)
                    indices.presentFamily = i;

                if (indices.isComplete()) break;

                i++;
            }

            return indices;
        }

        bool checkDeviceExtensionSupport(VkPhysicalDevice device)
        {
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());

            for (const auto& ext : availableExtensions)
                requiredExtensions.erase(ext.extensionName);

            return requiredExtensions.empty();
        }

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            SwapChainSupportDetails details;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
            if (formatCount > 0)
            {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
            if (presentModeCount > 0)
            {
                details.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
            }

            return details;
        }

        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            VkPhysicalDeviceProperties deviceProps;
            vkGetPhysicalDeviceProperties(device, &deviceProps);

            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            QueueFamilyIndices indices = findQueueFamilies(device, surface);

            bool extensionsSupported = checkDeviceExtensionSupport(device);

            bool swapChainAdequate = false;
            if (extensionsSupported)
            {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            bool isDiscreteGpu = deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

            return isDiscreteGpu && indices.isComplete() && extensionsSupported && swapChainAdequate;
        }

        VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
        {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            VkPhysicalDevice preferredDevice = VK_NULL_HANDLE;
            for (const auto& device : devices)
            {
                if (isDeviceSuitable(device, surface))
                {
                    preferredDevice = device;
                    break;
                }
            }

            if (!preferredDevice)
                throw std::runtime_error("Failed to find a suitable physical device!");
            return preferredDevice;
        }
    }

    VulkanContext::VulkanContext(SurfaceCreator surfaceCreator, std::vector<std::string>&& reqPlatformExtensions)
        : m_instance(detail::createInstance(std::forward<std::vector<std::string>>(reqPlatformExtensions)))
        , m_debugCallback(detail::createDebugCallback(m_instance))
        , m_surface(detail::createSurface(m_instance, surfaceCreator))
        , m_physicalDevice(detail::pickPhysicalDevice(m_instance, m_surface))
    {
        vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_physicalDeviceFeatures);
        vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);
    }

    VulkanContext::~VulkanContext()
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        detail::DestroyDebugReportCallbackEXT(m_instance, m_debugCallback, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }

    VkPhysicalDevice VulkanContext::getPhysicalDevice() const
    {
        return m_physicalDevice;
    }

    VkSurfaceKHR VulkanContext::getSurface() const
    {
        return m_surface;
    }

    VkDevice VulkanContext::createLogicalDevice(const VulkanQueueConfiguration& config) const
    {
        VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        createInfo.pEnabledFeatures        = &m_physicalDeviceFeatures;
        createInfo.queueCreateInfoCount    = static_cast<uint32_t>(config.getQueueCreateInfos().size());
        createInfo.pQueueCreateInfos       = config.getQueueCreateInfos().data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(detail::DeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = detail::DeviceExtensions.data();
        createInfo.enabledLayerCount       = detail::EnableValidationLayers ? static_cast<uint32_t>(detail::ValidationLayers.size()) : 0;
        createInfo.ppEnabledLayerNames     = detail::EnableValidationLayers ? detail::ValidationLayers.data() : nullptr;

        VkDevice device(VK_NULL_HANDLE);
        vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &device);
        return device;
    }

    bool VulkanContext::getQueueFamilyPresentationSupport(uint32_t familyIndex) const
    {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, familyIndex, m_surface, &presentSupport);
        return static_cast<bool>(presentSupport);
    }

    std::vector<VkQueueFamilyProperties> VulkanContext::getQueueFamilyProperties() const
    {
        return detail::getQueueFamilyProperties(m_physicalDevice);
    }

    QueueFamilyIndices VulkanContext::findQueueFamilies() const
    {
        return detail::findQueueFamilies(m_physicalDevice, m_surface);
    }

    SwapChainSupportDetails VulkanContext::querySwapChainSupport() const
    {
        return detail::querySwapChainSupport(m_physicalDevice, m_surface);
    }

    std::optional<uint32_t> VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties))
                return i;
        }

        return {};
    }

    std::optional<uint32_t> VulkanContext::findMemoryType(VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        return {};
    }

    std::optional<uint32_t> VulkanContext::findDeviceImageMemoryType(VkDevice device)
    {
        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = 1;
        imageInfo.extent.height = 1;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags         = 0;

        VkImage dummyImage(VK_NULL_HANDLE);
        vkCreateImage(device, &imageInfo, nullptr, &dummyImage);
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, dummyImage, &memRequirements);
        vkDestroyImage(device, dummyImage, nullptr);

        return findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    std::optional<uint32_t> VulkanContext::findDeviceBufferMemoryType(VkDevice device)
    {
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size        = 1;
        bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer dummyBuffer(VK_NULL_HANDLE);
        vkCreateBuffer(device, &bufferInfo, nullptr, &dummyBuffer);
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, dummyBuffer, &memRequirements);
        vkDestroyBuffer(device, dummyBuffer, nullptr);

        return findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    std::optional<uint32_t> VulkanContext::findStagingBufferMemoryType(VkDevice device)
    {
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size        = 1;
        bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer dummyBuffer(VK_NULL_HANDLE);
        vkCreateBuffer(device, &bufferInfo, nullptr, &dummyBuffer);
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, dummyBuffer, &memRequirements);
        vkDestroyBuffer(device, dummyBuffer, nullptr);

        return findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    VkFormat VulkanContext::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
                return format;

            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
                return format;
        }

        std::cerr << "Could not find supported format!\n";
        return VkFormat();
    }

    VkFormat VulkanContext::findSupportedDepthFormat() const
    {
        return findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    const VkPhysicalDeviceFeatures& VulkanContext::getPhysicalDeviceFeatures() const
    {
        return m_physicalDeviceFeatures;
    }

    const VkPhysicalDeviceLimits& VulkanContext::getPhysicalDeviceLimits() const
    {
        return m_physicalDeviceProperties.limits;
    }

    const VkPhysicalDeviceProperties& VulkanContext::getPhysicalDeviceProperties() const
    {
        return m_physicalDeviceProperties;
    }
}