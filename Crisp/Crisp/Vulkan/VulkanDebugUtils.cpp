#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

namespace crisp {
namespace {
auto logger = createLoggerMt("VulkanDebug");

} // namespace

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