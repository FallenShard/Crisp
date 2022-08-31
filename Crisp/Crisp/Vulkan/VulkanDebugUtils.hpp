#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator);

VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance);
} // namespace crisp