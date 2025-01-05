#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

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
