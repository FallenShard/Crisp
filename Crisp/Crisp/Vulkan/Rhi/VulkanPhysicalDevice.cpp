
#include <Crisp/Vulkan/Rhi/VulkanPhysicalDevice.hpp>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {

CRISP_MAKE_LOGGER_ST("VulkanPhysicalDevice");

template <typename T, typename U>
void append(T& chainHead, U& newElement) {
    newElement.pNext = chainHead.pNext;
    chainHead.pNext = &newElement;
}

const char* getDeviceTypeString(const VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    default:
        return "Other";
    }
}

void logSelectedDevice(const VulkanPhysicalDevice& physicalDevice) {
    const auto apiVersion = physicalDevice.getProperties().apiVersion;
    CRISP_LOGI(
        "Selected device: {}, type: {}",
        physicalDevice.getProperties().deviceName,
        getDeviceTypeString(physicalDevice.getProperties().deviceType));
    CRISP_LOGI(
        " - API version:    {}.{}.{}",
        VK_VERSION_MAJOR(apiVersion),
        VK_VERSION_MINOR(apiVersion),
        VK_VERSION_PATCH(apiVersion));
    const bool isNvidia = physicalDevice.getProperties().vendorID == 0x10DE;
    const auto driverVersion = physicalDevice.getProperties().driverVersion;
    if (isNvidia) {
        CRISP_LOGI(
            " - Driver version: {}.{}.{}.{}",
            (driverVersion >> 22) & 0x3FF,
            (driverVersion >> 14) & 0xFF,
            (driverVersion >> 6) & 0xFF,
            driverVersion & 0x3F);
    } else {
        CRISP_LOGI("  Driver version: {}", driverVersion);
    }
}

} // namespace

VulkanPhysicalDevice::VulkanPhysicalDevice(const VkPhysicalDevice handle, const DeviceRequirements& requirements)
    : m_handle(handle)
    , m_properties(std::make_unique<Properties>()) {
    append(m_properties->features, m_properties->features11);
    append(m_properties->features, m_properties->features12);
    append(m_properties->features, m_properties->features13);
    append(m_properties->features, m_properties->features14);
    if (requirements.rayTracing) {
        append(m_properties->features, m_properties->rayTracingFeatures);
        append(m_properties->features, m_properties->accelerationStructureFeatures);
    }
    if (requirements.pageableMemory) {
        append(m_properties->features, m_properties->pageableDeviceLocalMemoryFeatures);
    }
    vkGetPhysicalDeviceFeatures2(m_handle, &m_properties->features);

    append(m_properties->properties, m_properties->properties11);
    append(m_properties->properties, m_properties->properties12);
    append(m_properties->properties, m_properties->properties13);
    append(m_properties->properties, m_properties->properties14);
    if (requirements.rayTracing) {
        append(m_properties->properties, m_properties->rayTracingProperties);
    }
    vkGetPhysicalDeviceProperties2(m_handle, &m_properties->properties);

    vkGetPhysicalDeviceMemoryProperties2(m_handle, &m_properties->memoryProperties);
}

bool VulkanPhysicalDevice::isSuitable(
    const VkSurfaceKHR surface, const std::vector<std::string>& deviceExtensions) const {
    if (!queryQueueFamilyIndices(surface).isComplete()) {
        return false;
    }

    if (!supportsDeviceExtensions(deviceExtensions)) {
        return false;
    }

    if (getProperties().deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        return false;
    }

    const SurfaceSupport surfaceSupport = querySurfaceSupport(surface);
    return !surfaceSupport.formats.empty() && !surfaceSupport.presentModes.empty();
}

bool VulkanPhysicalDevice::supportsPresentation(const uint32_t queueFamilyIndex, const VkSurfaceKHR surface) const {
    if (surface == VK_NULL_HANDLE) {
        return false;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_handle, queueFamilyIndex, surface, &presentSupport);
    return static_cast<bool>(presentSupport);
}

QueueFamilyIndices VulkanPhysicalDevice::queryQueueFamilyIndices(const VkSurfaceKHR surface) const {
    QueueFamilyIndices indices;

    const auto queueFamilies = queryQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& queueFamily = queueFamilies[i];

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            indices.transferFamily = i;
        }

        if (queueFamily.queueCount > 0 && supportsPresentation(i, surface)) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            return indices;
        }
    }

    return indices;
}

SurfaceSupport VulkanPhysicalDevice::querySurfaceSupport(const VkSurfaceKHR surface) const {
    SurfaceSupport surfaceSupport = {surface};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_handle, surface, &surfaceSupport.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface, &formatCount, nullptr);
    if (formatCount > 0) {
        surfaceSupport.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface, &formatCount, surfaceSupport.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        surfaceSupport.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_handle, surface, &presentModeCount, surfaceSupport.presentModes.data());
    }

    return surfaceSupport;
}

std::vector<VkQueueFamilyProperties> VulkanPhysicalDevice::queryQueueFamilyProperties() const {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount, queueFamilies.data());

    return queueFamilies;
}

Result<uint32_t> VulkanPhysicalDevice::findMemoryType(
    const uint32_t memoryTypeMask, const VkMemoryPropertyFlags properties) const {
    const VkPhysicalDeviceMemoryProperties& memProperties = getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memoryTypeMask & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)) {
            return i;
        }
    }

    return resultError("Unable to find memory type with filter {} and props {}!", memoryTypeMask, properties);
}

Result<uint32_t> VulkanPhysicalDevice::findMemoryType(const VkMemoryPropertyFlags properties) const {
    const VkPhysicalDeviceMemoryProperties& memProperties = getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return resultError("Unable to find memory type with props {}!", properties);
}

Result<uint32_t> VulkanPhysicalDevice::findDeviceImageMemoryType(const VkDevice device) const {
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = 1;
    imageInfo.extent.height = 1;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    VkImage dummyImage(VK_NULL_HANDLE);
    vkCreateImage(device, &imageInfo, nullptr, &dummyImage);
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, dummyImage, &memRequirements);
    vkDestroyImage(device, dummyImage, nullptr);

    return findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

Result<uint32_t> VulkanPhysicalDevice::findDeviceBufferMemoryType(const VkDevice device) const {
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = 1;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer dummyBuffer(VK_NULL_HANDLE);
    vkCreateBuffer(device, &bufferInfo, nullptr, &dummyBuffer);
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, dummyBuffer, &memRequirements);
    vkDestroyBuffer(device, dummyBuffer, nullptr);

    return findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

Result<uint32_t> VulkanPhysicalDevice::findStagingBufferMemoryType(const VkDevice device) const {
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = 1;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer dummyBuffer(VK_NULL_HANDLE);
    vkCreateBuffer(device, &bufferInfo, nullptr, &dummyBuffer);
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, dummyBuffer, &memRequirements);
    vkDestroyBuffer(device, dummyBuffer, nullptr);

    return findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

bool VulkanPhysicalDevice::supportsDeviceExtensions(const std::vector<std::string>& deviceExtensions) const {
    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &extensionCount, availableExtensions.data()));

    FlatHashSet<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : availableExtensions) {
        requiredExtensions.erase(ext.extensionName); // NOLINT
    }

    return requiredExtensions.empty();
}

void VulkanPhysicalDevice::setDeviceExtensions(std::vector<std::string>&& deviceExtensions) {
    m_deviceExtensions = std::move(deviceExtensions);
}

Result<VkFormat> VulkanPhysicalDevice::findSupportedFormat(
    const std::vector<VkFormat>& candidates, const VkImageTiling tiling, const VkFormatFeatureFlags features) const {
    for (const auto& format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_handle, format, &props);

        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
    }

    return resultError("Could not find a supported format!");
}

Result<VkFormat> VulkanPhysicalDevice::findSupportedDepthFormat() const {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormatProperties VulkanPhysicalDevice::getFormatProperties(VkFormat format) const {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_handle, format, &props);
    return props;
}

const std::vector<std::string>& VulkanPhysicalDevice::getDeviceExtensions() const {
    return m_deviceExtensions;
}

std::vector<std::string> createDefaultDeviceExtensions() {
    return {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
}

void addPageableMemoryDeviceExtensions(std::vector<std::string>& deviceExtensions) {
    deviceExtensions.emplace_back(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);
    deviceExtensions.emplace_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
}

void addRayTracingDeviceExtensions(std::vector<std::string>& deviceExtensions) {
    deviceExtensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    deviceExtensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    deviceExtensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    deviceExtensions.emplace_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
}

Result<VulkanPhysicalDevice> selectPhysicalDevice(
    const VulkanInstance& instance, std::vector<std::string>&& deviceExtensions) {
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance.getHandle(), &deviceCount, nullptr));
    CRISP_CHECK_GE(deviceCount, 0, "Vulkan found no physical devices.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance.getHandle(), &deviceCount, devices.data()));

    if (instance.getSurface() == VK_NULL_HANDLE) {
        return VulkanPhysicalDevice(devices.front());
    }

    if (!deviceExtensions.empty()) {
        CRISP_LOGI("Requesting {} device extensions.", deviceExtensions.size());
        for (const auto& ext : deviceExtensions) {
            CRISP_LOGI(" - {}", ext);
        }
    }

    for (const auto& device : devices) {
        VulkanPhysicalDevice physicalDevice(device);
        if (physicalDevice.isSuitable(instance.getSurface(), deviceExtensions)) {
            logSelectedDevice(physicalDevice);
            physicalDevice.setDeviceExtensions(std::move(deviceExtensions));
            return physicalDevice;
        }
    }

    return resultError("Failed to find a suitable physical device!");
}
} // namespace crisp