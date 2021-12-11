#include <Crisp/Vulkan/VulkanContext.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>
#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

#include <CrispCore/Logger.hpp>

#include <algorithm>
#include <unordered_set>

namespace crisp
{
    namespace
    {
        auto logger = createLoggerMt("VulkanContext");
    
        const std::vector<const char*> ValidationLayers =
        {
            "VK_LAYER_KHRONOS_validation"
        };

        [[nodiscard]] Result<> assertRequiredExtensionSupport(const std::vector<const char*>& requiredExtensions, const std::vector<VkExtensionProperties>& supportedExtensions)
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

                return resultError("Failed to support required extensions. Aborting the application.");
            }

            return {};
        }

        [[nodiscard]] Result<> assertValidationLayerSupport(const bool validationLayersEnabled)
        {
            if (!validationLayersEnabled)
                return {};

            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            std::unordered_set<std::string> availableLayersSet;
            for (const auto& layer : availableLayers)
                availableLayersSet.insert(layer.layerName);

            for (const auto& validationLayer : ValidationLayers)
                if (availableLayersSet.find(validationLayer) == availableLayersSet.end())
                    return resultError("Validation layer {} is requested, but not supported!", validationLayer);

            logger->info("Validation layers are supported!");
            return {};
        }

        VkInstance createInstance(std::vector<std::string>&& reqPlatformExtensions, const bool enableValidationLayers)
        {
            VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
            appInfo.pApplicationName   = Application::Title;
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName        = "CrispEngine";
            appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion         = VK_API_VERSION_1_2;

            if (enableValidationLayers)
                reqPlatformExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            std::vector<const char*> enabledExtensions;
            std::transform(reqPlatformExtensions.begin(), reqPlatformExtensions.end(), std::back_inserter(enabledExtensions), [](const auto& ext) { return ext.c_str(); });

            VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
            createInfo.pApplicationInfo        = &appInfo;
            createInfo.enabledExtensionCount   = static_cast<uint32_t>(enabledExtensions.size());
            createInfo.ppEnabledExtensionNames = enabledExtensions.data();
            createInfo.enabledLayerCount       = enableValidationLayers ? static_cast<uint32_t>(ValidationLayers.size()) : 0;
            createInfo.ppEnabledLayerNames     = enableValidationLayers ? ValidationLayers.data() : nullptr;

            VkInstance instance;
            vkCreateInstance(&createInfo, nullptr, &instance);

            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> extensionProps(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProps.data());

            assertRequiredExtensionSupport(enabledExtensions, extensionProps).unwrap();
            assertValidationLayerSupport(enableValidationLayers).unwrap();

            return instance;
        }

        VkSurfaceKHR createSurface(const VkInstance instance, const SurfaceCreator surfaceCreator)
        {
            VkSurfaceKHR surface;
            surfaceCreator(instance, nullptr, &surface);
            return surface;
        }
    }

    VulkanContext::VulkanContext(SurfaceCreator surfaceCreator, std::vector<std::string>&& platformExtensions, const bool enableValidationLayers)
        : m_instance(createInstance(std::move(platformExtensions), enableValidationLayers))
        , m_debugMessenger(enableValidationLayers ? createDebugMessenger(m_instance) : VK_NULL_HANDLE)
        , m_surface(surfaceCreator ? createSurface(m_instance, surfaceCreator) : VK_NULL_HANDLE)
    {
    }

    VulkanContext::~VulkanContext()
    {
        if (m_instance == VK_NULL_HANDLE)
            return;

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }

    Result<VulkanPhysicalDevice> VulkanContext::selectPhysicalDevice(std::vector<std::string>&& deviceExtensions) const
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0)
            return resultError("Vulkan found no physical devices. Device count: {}", deviceCount);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        if (m_surface == VK_NULL_HANDLE)
        {
            return VulkanPhysicalDevice(devices.front());
        }

        std::vector<const char*> requestedDeviceExtensions;
        std::transform(deviceExtensions.begin(), deviceExtensions.end(), std::back_inserter(requestedDeviceExtensions), [](const std::string& ext)
        {
            return ext.c_str();
        });

        for (const auto& device : devices)
        {
            VulkanPhysicalDevice physicalDevice(device);
            if (physicalDevice.isSuitable(m_surface, requestedDeviceExtensions))
            {
                physicalDevice.setDeviceExtensions(std::move(deviceExtensions));
                return physicalDevice;
            }
        }

        return resultError("Failed to find a suitable physical device!");
    }

    VkSurfaceKHR VulkanContext::getSurface() const
    {
        return m_surface;
    }
}