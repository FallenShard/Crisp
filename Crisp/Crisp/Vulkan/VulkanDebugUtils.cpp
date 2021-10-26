#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace crisp
{
    namespace
    {
        auto logger = spdlog::stderr_color_mt("VulkanDebug");

        VkBool32 debugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
        {
            const char* typeStr = "Unknown";
            if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
                typeStr = "General";
            if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                typeStr = "Performance";
            if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
                typeStr = "Validation";

            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
                logger->debug("{} {} {} {}", typeStr, callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
                logger->info("{} {} {} {}", typeStr, callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                logger->warn("{} {} {} {}", typeStr, callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                logger->error("{} {} {} {}", typeStr, callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
            else
                logger->error("{} {} {} {}", "Unknown", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
            return VK_FALSE;
        }
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
        const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
            return func(instance, createInfo, allocator, messenger);
        else
            return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
    {
        if (messenger == nullptr)
            return;

        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, messenger, pAllocator);
    }

    VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance)
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        createInfo.flags = 0;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        createInfo.pfnUserCallback = debugMessengerCallback;

        VkDebugUtilsMessengerEXT callback;
        CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback);
        logger->info("Debug messenger created!");
        return callback;
    }
}