#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

namespace crisp {
namespace {
auto logger = createLoggerMt("VulkanDebug");
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
        logger->debug("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        logger->info("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        logger->warn("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    } else {
        logger->error("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
        // std::terminate();
    }

    return VK_FALSE;
}

} // namespace

VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance) {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    createInfo.flags = 0;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.pfnUserCallback = debugMessengerCallback;

    VkDebugUtilsMessengerEXT callback{VK_NULL_HANDLE};
    vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback);
    return callback;
}

VulkanDebugMarker::VulkanDebugMarker(const VkDevice device)
    : m_device(device) {}

void VulkanDebugMarker::setObjectName(
    const uint64_t vulkanHandle, const char* name, const VkObjectType objectType) const {
    VkDebugUtilsObjectNameInfoEXT nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = vulkanHandle;
    nameInfo.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(m_device, &nameInfo);
}

} // namespace crisp