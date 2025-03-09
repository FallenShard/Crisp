#include <Crisp/Vulkan/Rhi/VulkanInstance.hpp>

#include <ranges>
#include <span>
#include <unordered_set>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_ST("VulkanInstance");

const std::vector<const char*> kValidationLayers = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};

bool printAllDebugMessages{false};

VkBool32 debugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/) {
    const char* typeStr = "Unknown";
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        typeStr = "General";
    }
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        typeStr = "Performance";
    }
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        typeStr = "Validation";
    }
    if (strcmp(typeStr, "General") == 0 && !printAllDebugMessages) {
        return VK_FALSE;
    }

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        CRISP_LOGD("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        CRISP_LOGI("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        CRISP_LOGW("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    } else {
        CRISP_LOGE("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
        // std::terminate();
    }

    return VK_FALSE;
}

Result<> assertRequiredExtensionSupport(const std::span<const char* const> requiredExtensions) {
    CRISP_LOGI("Requesting {} instance extensions.", requiredExtensions.size());
    std::unordered_set<std::string> pendingExtensions;
    for (const auto* const extensionName : requiredExtensions) {
        CRISP_LOGI(" - {}", extensionName);
        pendingExtensions.insert(std::string(extensionName));
    }

    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> extensionProps(extensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProps.data()));

    for (const auto& ext : extensionProps) {
        pendingExtensions.erase(ext.extensionName); // Will hold unsupported required extensions, if any. // NOLINT
    }

    if (!pendingExtensions.empty()) {
        const size_t numSupportedReqExts{requiredExtensions.size() - pendingExtensions.size()};
        CRISP_LOGW("{}/{} required extensions supported.", numSupportedReqExts, requiredExtensions.size());
        CRISP_LOGE("The following required extensions are not supported:");
        for (const auto& ext : pendingExtensions) {
            CRISP_LOGE(" - {}", ext);
        }

        return resultError("Failed to support required extensions. Aborting the application.");
    }

    return {};
}

Result<> assertRequiredLayerSupport(const std::span<const char* const> requiredLayers) {
    CRISP_LOGI("Requesting {} instance layers.", requiredLayers.size());
    std::unordered_set<std::string> pendingLayers;
    for (const auto* const layerName : requiredLayers) {
        CRISP_LOGI(" - {}", layerName);
        pendingLayers.insert(std::string(layerName));
    }

    uint32_t layerCount{0};
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

    std::vector<VkLayerProperties> instanceLayers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, instanceLayers.data()));

    for (const auto& layer : instanceLayers) {
        pendingLayers.erase(layer.layerName); // NOLINT
    }

    if (!pendingLayers.empty()) {
        const size_t supportedAndRequiredLayerCount{requiredLayers.size() - pendingLayers.size()};
        CRISP_LOGW("{}/{} required layers supported.", supportedAndRequiredLayerCount, requiredLayers.size());
        CRISP_LOGE("The following required layers are not supported:");
        for (const auto& ext : pendingLayers) {
            CRISP_LOGE(" - {}", ext);
        }

        return resultError("Failed to support required layers. Aborting...");
    }

    return {};
}

VkInstance createInstance(std::vector<std::string> requiredExtensions, const bool enableValidationLayers) { // NOLINT
    loadVulkanLoaderFunctions();
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Crisp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CrispEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

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

VkDebugUtilsMessengerEXT createDebugMessenger(const VkInstance instance) {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    createInfo.flags = 0;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.pfnUserCallback = debugMessengerCallback;

    VkDebugUtilsMessengerEXT callback{VK_NULL_HANDLE};
    vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback);
    return callback;
}

VkSurfaceKHR createSurface(const VkInstance instance, const SurfaceCreator& surfaceCreator) {
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VK_CHECK(surfaceCreator(instance, nullptr, &surface));
    return surface;
}
} // namespace

VulkanInstance::VulkanInstance(
    const SurfaceCreator& surfaceCreator, std::vector<std::string> requiredExtensions, const bool enableValidationLayers)
    : m_handle(createInstance(requiredExtensions, enableValidationLayers))
    , m_debugMessenger(enableValidationLayers ? createDebugMessenger(m_handle) : VK_NULL_HANDLE)
    , m_surface(surfaceCreator ? createSurface(m_handle, surfaceCreator) : VK_NULL_HANDLE)
    , m_apiVersion(VK_API_VERSION_1_4)
    , m_enabledExtensions(requiredExtensions.begin(), requiredExtensions.end()) {
    if (enableValidationLayers) {
        m_enabledExtensions.emplace(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        m_enabledLayers.insert(kValidationLayers.begin(), kValidationLayers.end());
    }
}

VulkanInstance::~VulkanInstance() {
    if (m_handle == VK_NULL_HANDLE) {
        return;
    }

    if (m_surface) {
        vkDestroySurfaceKHR(m_handle, m_surface, nullptr);
    }
    if (m_debugMessenger) {
        vkDestroyDebugUtilsMessengerEXT(m_handle, m_debugMessenger, nullptr);
    }
    vkDestroyInstance(m_handle, nullptr);
}

VkInstance VulkanInstance::getHandle() const {
    return m_handle;
}

VkSurfaceKHR VulkanInstance::getSurface() const {
    return m_surface;
}

uint32_t VulkanInstance::getApiVersion() const {
    return m_apiVersion;
}

bool VulkanInstance::isExtensionEnabled(const std::string_view extensionName) const {
    return m_enabledExtensions.contains(extensionName);
}

bool VulkanInstance::isLayerEnabled(const std::string_view layerName) const {
    return m_enabledLayers.contains(layerName);
}

} // namespace crisp