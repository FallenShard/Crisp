#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

namespace crisp
{
namespace
{
auto logger = createLoggerMt("VulkanDebug");

VkBool32 debugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/)
{
    const char* typeStr = "Unknown";
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        typeStr = "General";
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        typeStr = "Performance";
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        typeStr = "Validation";

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        logger->debug("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        logger->info("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        logger->warn("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    }
    else
    {
        logger->error("{} {} {} \n{}", typeStr, data->messageIdNumber, data->pMessageIdName, data->pMessage);
    }

    return VK_FALSE;
}

bool debugMarkerPointersRetrieved{false};
PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTag;
PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectName;
PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabel;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabel;
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabel;

} // namespace

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* messenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(instance, createInfo, allocator, messenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
{
    if (messenger == nullptr)
        return;

    const auto func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance, messenger, pAllocator);
}

VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance)
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    createInfo.flags = 0;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.pfnUserCallback = debugMessengerCallback;

    VkDebugUtilsMessengerEXT callback;
    CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback);
    return callback;
}

VulkanDebugMarker::VulkanDebugMarker(const VkDevice device)
    : m_device(device)
{
    if (!debugMarkerPointersRetrieved)
    {
        vkSetDebugUtilsObjectTag =
            (PFN_vkSetDebugUtilsObjectTagEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectTagEXT");
        vkSetDebugUtilsObjectName =
            (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
        vkCmdBeginDebugUtilsLabel =
            (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT");
        vkCmdEndDebugUtilsLabel =
            (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");
        vkCmdInsertDebugUtilsLabel =
            (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT");
        debugMarkerPointersRetrieved = true;
    }
}

void VulkanDebugMarker::setObjectName(
    const uint64_t vulkanHandle, const char* name, const VkObjectType objectType) const
{
    VkDebugUtilsObjectNameInfoEXT nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = vulkanHandle;
    nameInfo.pObjectName = name;
    vkSetDebugUtilsObjectName(m_device, &nameInfo);
}

} // namespace crisp