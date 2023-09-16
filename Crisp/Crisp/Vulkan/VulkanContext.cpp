#include <Crisp/Vulkan/VulkanContext.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/VulkanChecks.hpp>
#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

#include <unordered_set>

namespace crisp
{
namespace
{
auto logger = createLoggerMt("VulkanContext");

const std::vector<const char*> ValidationLayers = {"VK_LAYER_KHRONOS_validation"};

[[nodiscard]] Result<> assertRequiredExtensionSupport(
    const std::vector<const char*>& requiredExtensions, const std::vector<VkExtensionProperties>& supportedExtensions)
{
    std::unordered_set<std::string> pendingExtensions;

    logger->info("Identified {} required Vulkan extensions:", requiredExtensions.size());
    for (const auto* const extensionName : requiredExtensions)
    {
        logger->info("\t{}", extensionName);
        pendingExtensions.insert(std::string(extensionName));
    }

    for (const auto& ext : supportedExtensions)
    {
        pendingExtensions.erase(ext.extensionName); // Will hold unsupported required extensions, if any. // NOLINT
    }

    const size_t numSupportedReqExts{requiredExtensions.size() - pendingExtensions.size()};
    logger->info("{}/{} required extensions supported.", numSupportedReqExts, requiredExtensions.size());

    if (!pendingExtensions.empty())
    {
        logger->error("The following required extensions are not supported:");
        for (const auto& ext : pendingExtensions)
        {
            logger->error("{}", ext);
        }

        return resultError("Failed to support required extensions. Aborting the application.");
    }

    return {};
}

[[nodiscard]] Result<> assertValidationLayerSupport(const bool validationLayersEnabled)
{
    if (!validationLayersEnabled)
    {
        return {};
    }

    uint32_t layerCount{0};
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

    std::vector<VkLayerProperties> availableLayers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()));

    std::unordered_set<std::string> availableLayersSet;
    for (const auto& layer : availableLayers)
    {
        availableLayersSet.insert(layer.layerName); // NOLINT
    }

    for (const auto& validationLayer : ValidationLayers)
    {
        if (availableLayersSet.find(validationLayer) == availableLayersSet.end())
        {
            return resultError("Validation layer {} is requested, but not supported!", validationLayer);
        }
    }

    logger->info("Validation layers are supported!");
    return {};
}

VkInstance createInstance(std::vector<std::string>&& reqPlatformExtensions, const bool enableValidationLayers)
{
    loadVulkanLoaderFunctions();
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Crisp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CrispEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    if (enableValidationLayers)
    {
        reqPlatformExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    std::vector<const char*> enabledExtensions;
    std::transform(
        reqPlatformExtensions.begin(),
        reqPlatformExtensions.end(),
        std::back_inserter(enabledExtensions),
        [](const auto& ext) { return ext.c_str(); });

    VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    createInfo.enabledLayerCount = enableValidationLayers ? static_cast<uint32_t>(ValidationLayers.size()) : 0;
    createInfo.ppEnabledLayerNames = enableValidationLayers ? ValidationLayers.data() : nullptr;

    VkInstance instance{VK_NULL_HANDLE};
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> extensionProps(extensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProps.data()));

    assertRequiredExtensionSupport(enabledExtensions, extensionProps).unwrap();
    assertValidationLayerSupport(enableValidationLayers).unwrap();

    loadVulkanInstanceFunctions(instance);
    return instance;
}

VkSurfaceKHR createSurface(const VkInstance instance, SurfaceCreator&& surfaceCreator)
{
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VK_CHECK(surfaceCreator(instance, nullptr, &surface));
    return surface;
}
} // namespace

VulkanContext::VulkanContext(
    SurfaceCreator&& surfaceCreator, std::vector<std::string>&& platformExtensions, const bool enableValidationLayers)
    : m_instance(createInstance(std::move(platformExtensions), enableValidationLayers))
    , m_debugMessenger(enableValidationLayers ? createDebugMessenger(m_instance) : VK_NULL_HANDLE)
    , m_surface(surfaceCreator ? createSurface(m_instance, std::move(surfaceCreator)) : VK_NULL_HANDLE)
{
}

VulkanContext::~VulkanContext()
{
    if (m_instance == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_surface)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    if (m_debugMessenger)
    {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }
    vkDestroyInstance(m_instance, nullptr);
}

Result<VulkanPhysicalDevice> VulkanContext::selectPhysicalDevice(std::vector<std::string>&& deviceExtensions) const
{
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
    CRISP_CHECK(deviceCount > 0, "Vulkan found no physical devices.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()));

    if (m_surface == VK_NULL_HANDLE)
    {
        return VulkanPhysicalDevice(devices.front());
    }

    for (const auto& device : devices)
    {
        VulkanPhysicalDevice physicalDevice(device);
        if (physicalDevice.isSuitable(m_surface, deviceExtensions))
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

VkInstance VulkanContext::getInstance() const
{
    return m_instance;
}

} // namespace crisp