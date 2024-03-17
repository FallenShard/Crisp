#include <Crisp/Vulkan/VulkanContext.hpp>

#include <ranges>
#include <span>
#include <unordered_set>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/VulkanChecks.hpp>
#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

namespace crisp {
namespace {
auto logger = createLoggerMt("VulkanContext");

const std::vector<const char*> kValidationLayers = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};

Result<> assertRequiredExtensionSupport(const std::span<const char* const> requiredExtensions) {
    logger->trace("Requesting {} instance extensions:", requiredExtensions.size());
    std::unordered_set<std::string> pendingExtensions;
    for (const auto* const extensionName : requiredExtensions) {
        logger->trace("    {}", extensionName);
        pendingExtensions.insert(std::string(extensionName));
    }

    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> extensionProps(extensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProps.data()));

    for (const auto& ext : extensionProps) {
        pendingExtensions.erase(ext.extensionName); // Will hold unsupported required extensions, if any. // NOLINT
    }

    const size_t numSupportedReqExts{requiredExtensions.size() - pendingExtensions.size()};
    logger->info("{}/{} required extensions supported.", numSupportedReqExts, requiredExtensions.size());

    if (!pendingExtensions.empty()) {
        logger->error("The following required extensions are not supported:");
        for (const auto& ext : pendingExtensions) {
            logger->error("    {}", ext);
        }

        return resultError("Failed to support required extensions. Aborting the application.");
    }

    return {};
}

Result<> assertRequiredLayerSupport(const std::span<const char* const> requiredLayers) {
    logger->trace("Requesting {} instance layers:", requiredLayers.size());
    std::unordered_set<std::string> pendingLayers;
    for (const auto* const layerName : requiredLayers) {
        logger->trace("    {}", layerName);
        pendingLayers.insert(std::string(layerName));
    }

    uint32_t layerCount{0};
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

    std::vector<VkLayerProperties> instanceLayers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, instanceLayers.data()));

    for (const auto& layer : instanceLayers) {
        pendingLayers.erase(layer.layerName); // NOLINT
    }

    const size_t supportedAndRequiredLayerCount{requiredLayers.size() - pendingLayers.size()};
    logger->info("{}/{} required layers supported.", supportedAndRequiredLayerCount, requiredLayers.size());

    if (!pendingLayers.empty()) {
        logger->error("The following required layers are not supported:");
        for (const auto& ext : pendingLayers) {
            logger->error("    {}", ext);
        }

        return resultError("Failed to support required layers. Aborting...");
    }

    return {};
}

VkInstance createInstance(std::vector<std::string>&& requiredExtensions, const bool enableValidationLayers) { // NOLINT
    loadVulkanLoaderFunctions();
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Crisp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CrispEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    if (enableValidationLayers) {
        requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    const auto enabledExtensions =
        std::views::transform(requiredExtensions, [](const auto& ext) { return ext.c_str(); }) |
        std::ranges::to<std::vector<const char*>>();
    assertRequiredExtensionSupport(enabledExtensions).unwrap();

    const std::vector<const char*> requiredLayers =
        enableValidationLayers ? kValidationLayers : std::vector<const char*>{};
    assertRequiredLayerSupport(requiredLayers).unwrap();

    VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
    createInfo.ppEnabledLayerNames = requiredLayers.data();

    VkInstance instance{VK_NULL_HANDLE};
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

    loadVulkanInstanceFunctions(instance);
    return instance;
}

VkSurfaceKHR createSurface(const VkInstance instance, const SurfaceCreator& surfaceCreator) {
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VK_CHECK(surfaceCreator(instance, nullptr, &surface));
    return surface;
}
} // namespace

VulkanContext::VulkanContext(
    const SurfaceCreator& surfaceCreator,
    std::vector<std::string>&& platformExtensions,
    const bool enableValidationLayers)
    : m_instance(createInstance(std::move(platformExtensions), enableValidationLayers))
    , m_debugMessenger(enableValidationLayers ? createDebugMessenger(m_instance) : VK_NULL_HANDLE)
    , m_surface(surfaceCreator ? createSurface(m_instance, surfaceCreator) : VK_NULL_HANDLE) {}

VulkanContext::~VulkanContext() {
    if (m_instance == VK_NULL_HANDLE) {
        return;
    }

    if (m_surface) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    if (m_debugMessenger) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }
    vkDestroyInstance(m_instance, nullptr);
}

Result<VulkanPhysicalDevice> VulkanContext::selectPhysicalDevice(std::vector<std::string>&& deviceExtensions) const {
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
    CRISP_CHECK_GE(deviceCount, 0, "Vulkan found no physical devices.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()));

    if (m_surface == VK_NULL_HANDLE) {
        return VulkanPhysicalDevice(devices.front());
    }

    for (const auto& device : devices) {
        VulkanPhysicalDevice physicalDevice(device);
        if (physicalDevice.isSuitable(m_surface, deviceExtensions)) {
            physicalDevice.setDeviceExtensions(std::move(deviceExtensions));
            return physicalDevice;
        }
    }

    return resultError("Failed to find a suitable physical device!");
}

VkSurfaceKHR VulkanContext::getSurface() const {
    return m_surface;
}

VkInstance VulkanContext::getInstance() const {
    return m_instance;
}

} // namespace crisp