#include <Crisp/Vulkan/VulkanHeader.hpp>

#define VOLK_IMPLEMENTATION
#include <volk.h>

namespace crisp {
VkResult loadVulkanLoaderFunctions() {
    return volkInitialize();
}

VkResult loadVulkanInstanceFunctions(const VkInstance instance) {
    volkLoadInstanceOnly(instance);
    return VK_SUCCESS;
}

VkResult loadVulkanDeviceFunctions(const VkDevice device) {
    volkLoadDevice(device);
    return VK_SUCCESS;
}
} // namespace crisp
