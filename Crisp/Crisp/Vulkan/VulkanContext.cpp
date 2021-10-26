#include <Crisp/Vulkan/VulkanContext.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>
#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <unordered_set>

namespace crisp
{
    namespace
    {
        auto logger = spdlog::stderr_color_mt("VulkanContext");
    }

    namespace detail
    {
#ifndef _DEBUG
        constexpr bool EnableValidationLayers = false;
#else
        constexpr bool EnableValidationLayers = true;
#endif

        const std::vector<const char*> ValidationLayers =
        {
            "VK_LAYER_KHRONOS_validation"
        };

        const std::vector<const char*> DeviceExtensions =
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            //VK_NV_RAY_TRACING_EXTENSION_NAME,
            VK_KHR_MAINTENANCE2_EXTENSION_NAME
        };

        void assertRequiredExtensionSupport(std::vector<const char*> requiredExtensions, const std::vector<VkExtensionProperties>& supportedExtensions)
        {
            std::unordered_set<std::string> pendingExtensions;

            logger->info("Platform-required Vulkan extensions ({}):", requiredExtensions.size());
            for (auto extensionName : requiredExtensions)
            {
                logger->info("\t{}", extensionName);
                pendingExtensions.insert(std::string(extensionName));
            }

            logger->info("Supported extensions ({}):", supportedExtensions.size());
            for (const auto& ext : supportedExtensions)
            {
                logger->info("\t{}", ext.extensionName);
                pendingExtensions.erase(ext.extensionName); // Will hold unsupported required extensions, if any
            }

            size_t numSupportedReqExts = requiredExtensions.size() - pendingExtensions.size();

            logger->info("{}/{} required extensions supported.", numSupportedReqExts, requiredExtensions.size());

            if (!pendingExtensions.empty())
            {
                logger->error("The following required extensions are not supported:");
                for (const auto& ext : pendingExtensions)
                    logger->error("{}", ext);

                logger->critical("Aborting the application.");
                std::exit(-1);
            }
        }

        void assertValidationLayerSupport()
        {
            if (!EnableValidationLayers)
                return;

            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            std::unordered_set<std::string> availableLayersSet;
            for (const auto& layer : availableLayers)
                availableLayersSet.insert(layer.layerName);

            for (const auto& validationLayer : ValidationLayers)
                if (availableLayersSet.find(validationLayer) == availableLayersSet.end())
                    logger->critical("Validation layer {} is not requested, but not supported!", validationLayer);

            logger->info("Validation layers are supported!");
        }

        VkInstance createInstance(std::vector<std::string>&& reqPlatformExtensions)
        {
            VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
            appInfo.pApplicationName   = Application::Title;
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName        = "CrispEngine";
            appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion         = VK_API_VERSION_1_2;

            if (EnableValidationLayers)
                reqPlatformExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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

            assertRequiredExtensionSupport(enabledExtensions, extensionProps);
            assertValidationLayerSupport();

            return instance;
        }



        VkSurfaceKHR createSurface(VkInstance instance, SurfaceCreator surfaceCreator)
        {
            VkSurfaceKHR surface;
            surfaceCreator(instance, nullptr, &surface);
            return surface;
        }

        VulkanPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
        {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            for (const auto& device : devices)
            {
                VulkanPhysicalDevice physicalDevice(device);
                if (physicalDevice.isSuitable(surface, DeviceExtensions))
                    return physicalDevice;
            }

            logger->critical("Failed to find a suitable physical device!");
            return VulkanPhysicalDevice(VK_NULL_HANDLE);
        }
    }

    VulkanContext::VulkanContext(SurfaceCreator surfaceCreator, std::vector<std::string>&& reqPlatformExtensions)
        : m_instance(detail::createInstance(std::forward<std::vector<std::string>>(reqPlatformExtensions)))
        , m_debugMessenger(detail::EnableValidationLayers ? createDebugMessenger(m_instance) : nullptr)
        , m_surface(detail::createSurface(m_instance, surfaceCreator))
        , m_physicalDevice(detail::pickPhysicalDevice(m_instance, m_surface))
    {
        logger->info("Constructed!");
    }

    VulkanContext::~VulkanContext()
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }

    VkSurfaceKHR VulkanContext::getSurface() const
    {
        return m_surface;
    }

    VkDevice VulkanContext::createLogicalDevice(const VulkanQueueConfiguration& config) const
    {
        VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        auto* ptr = &m_physicalDevice.getFeatures2();
        createInfo.pNext = nullptr;//ptr;
        createInfo.pEnabledFeatures = &m_physicalDevice.getFeatures();
        createInfo.queueCreateInfoCount    = static_cast<uint32_t>(config.getQueueCreateInfos().size());
        createInfo.pQueueCreateInfos       = config.getQueueCreateInfos().data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(detail::DeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = detail::DeviceExtensions.data();

        VkDevice device(VK_NULL_HANDLE);
        vkCreateDevice(m_physicalDevice.getHandle(), &createInfo, nullptr, &device);
        return device;
    }

    bool VulkanContext::getQueueFamilyPresentationSupport(uint32_t familyIndex) const
    {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice.getHandle(), familyIndex, m_surface, &presentSupport);
        return static_cast<bool>(presentSupport);
    }

    QueueFamilyIndices VulkanContext::queryQueueFamilies() const
    {
        return m_physicalDevice.queryQueueFamilyIndices(m_surface);
    }

    SurfaceSupport VulkanContext::querySurfaceSupport() const
    {
        return m_physicalDevice.querySurfaceSupport(m_surface);
    }

    std::optional<uint32_t> VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        const VkPhysicalDeviceMemoryProperties& memProperties = m_physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties))
                return i;
        }

        return {};
    }

    std::optional<uint32_t> VulkanContext::findMemoryType(VkMemoryPropertyFlags properties) const
    {
        const VkPhysicalDeviceMemoryProperties& memProperties = m_physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        return {};
    }

    std::optional<uint32_t> VulkanContext::findDeviceImageMemoryType(VkDevice device) const
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

    std::optional<uint32_t> VulkanContext::findDeviceBufferMemoryType(VkDevice device) const
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

    std::optional<uint32_t> VulkanContext::findStagingBufferMemoryType(VkDevice device) const
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
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice.getHandle(), format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
                return format;

            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
                return format;
        }

        logger->critical("Could not find a supported format");
        return VkFormat();
    }

    VkFormat VulkanContext::findSupportedDepthFormat() const
    {
        return findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }
}