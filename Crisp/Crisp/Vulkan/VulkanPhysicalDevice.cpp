#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <CrispCore/RobinHood.hpp>

namespace crisp
{
VulkanPhysicalDevice::VulkanPhysicalDevice(const VkPhysicalDevice handle)
    : m_handle(handle)
{
    initFeaturesAndProperties();
}

VulkanPhysicalDevice::VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept
    : m_handle(std::exchange(other.m_handle, VK_NULL_HANDLE))
    , m_deviceExtensions(std::move(other.m_deviceExtensions))
{
    initFeaturesAndProperties();
}

VulkanPhysicalDevice& VulkanPhysicalDevice::operator=(VulkanPhysicalDevice&& other) noexcept
{
    m_handle = std::exchange(other.m_handle, VK_NULL_HANDLE);
    m_deviceExtensions = std::move(other.m_deviceExtensions);
    initFeaturesAndProperties();
    return *this;
}

bool VulkanPhysicalDevice::isSuitable(const VkSurfaceKHR surface,
    const std::vector<const char*>& deviceExtensions) const
{
    if (!queryQueueFamilyIndices(surface).isComplete())
        return false;

    if (!supportsDeviceExtensions(deviceExtensions))
        return false;

    if (getProperties().deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return false;

    const SurfaceSupport surfaceSupport = querySurfaceSupport(surface);
    if (surfaceSupport.formats.empty() || surfaceSupport.presentModes.empty())
        return false;

    return true;
}

bool VulkanPhysicalDevice::supportsPresentation(const uint32_t queueFamilyIndex, const VkSurfaceKHR surface) const
{
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_handle, queueFamilyIndex, surface, &presentSupport);
    return static_cast<bool>(presentSupport);
}

QueueFamilyIndices VulkanPhysicalDevice::queryQueueFamilyIndices(const VkSurfaceKHR surface) const
{
    QueueFamilyIndices indices;

    const auto queueFamilies = queryQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        const auto& queueFamily = queueFamilies[i];

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            indices.computeFamily = i;

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            indices.transferFamily = i;

        if (queueFamily.queueCount > 0 && supportsPresentation(i, surface))
            indices.presentFamily = i;

        if (indices.isComplete())
            return indices;
    }

    return indices;
}

SurfaceSupport VulkanPhysicalDevice::querySurfaceSupport(const VkSurfaceKHR surface) const
{
    SurfaceSupport surfaceSupport = { surface };
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_handle, surface, &surfaceSupport.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface, &formatCount, nullptr);
    if (formatCount > 0)
    {
        surfaceSupport.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface, &formatCount, surfaceSupport.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface, &presentModeCount, nullptr);
    if (presentModeCount > 0)
    {
        surfaceSupport.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface, &presentModeCount,
            surfaceSupport.presentModes.data());
    }

    return surfaceSupport;
}

std::vector<VkQueueFamilyProperties> VulkanPhysicalDevice::queryQueueFamilyProperties() const
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount, queueFamilies.data());

    return queueFamilies;
}

Result<uint32_t> VulkanPhysicalDevice::findMemoryType(const uint32_t memoryTypeMask,
    const VkMemoryPropertyFlags properties) const
{
    const VkPhysicalDeviceMemoryProperties& memProperties = getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((memoryTypeMask & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties))
            return i;
    }

    return resultError("Unable to find memory type with filter {} and props {}!", memoryTypeMask, properties);
}

Result<uint32_t> VulkanPhysicalDevice::findMemoryType(const VkMemoryPropertyFlags properties) const
{
    const VkPhysicalDeviceMemoryProperties& memProperties = getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    return resultError("Unable to find memory type with props {}!", properties);
}

Result<uint32_t> VulkanPhysicalDevice::findDeviceImageMemoryType(const VkDevice device) const
{
    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
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

Result<uint32_t> VulkanPhysicalDevice::findDeviceBufferMemoryType(const VkDevice device) const
{
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
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

Result<uint32_t> VulkanPhysicalDevice::findStagingBufferMemoryType(const VkDevice device) const
{
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
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

bool VulkanPhysicalDevice::supportsDeviceExtensions(const std::vector<const char*>& deviceExtensions) const
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &extensionCount, availableExtensions.data());

    robin_hood::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : availableExtensions)
        requiredExtensions.erase(ext.extensionName);

    return requiredExtensions.empty();
}

void VulkanPhysicalDevice::setDeviceExtensions(std::vector<std::string>&& deviceExtensions)
{
    m_deviceExtensions = std::move(deviceExtensions);
}

VkDevice VulkanPhysicalDevice::createLogicalDevice(const VulkanQueueConfiguration& config) const
{
    std::vector<const char*> enabledExtensions;
    std::transform(m_deviceExtensions.begin(), m_deviceExtensions.end(), std::back_inserter(enabledExtensions),
        [](const std::string& ext)
        {
            return ext.c_str();
        });

    VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.pNext = &m_features;
    // createInfo.pEnabledFeatures        = &m_features.features;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(config.createInfos.size());
    createInfo.pQueueCreateInfos = config.createInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VkDevice device(VK_NULL_HANDLE);
    vkCreateDevice(m_handle, &createInfo, nullptr, &device);
    return device;
}

Result<VkFormat> VulkanPhysicalDevice::findSupportedFormat(const std::vector<VkFormat>& candidates,
    const VkImageTiling tiling, const VkFormatFeatureFlags features) const
{
    for (const auto& format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_handle, format, &props);

        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
    }

    return resultError("Could not find a supported format!");
}

Result<VkFormat> VulkanPhysicalDevice::findSupportedDepthFormat() const
{
    return findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void VulkanPhysicalDevice::initFeaturesAndProperties()
{
    m_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    m_properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    m_rayTracingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
    m_accelerationStructureFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    m_memoryProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    m_features11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    m_features12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    m_properties11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES };
    m_properties12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
    m_rayTracingPipelineProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

    m_features.pNext = &m_features11;
    m_features11.pNext = &m_features12;
    m_features12.pNext = &m_rayTracingFeatures;
    m_rayTracingFeatures.pNext = &m_accelerationStructureFeatures;
    vkGetPhysicalDeviceFeatures2(m_handle, &m_features);

    m_properties.pNext = &m_rayTracingPipelineProperties;
    m_rayTracingPipelineProperties.pNext = &m_properties11;
    m_properties11.pNext = &m_properties12;
    vkGetPhysicalDeviceProperties2(m_handle, &m_properties);

    vkGetPhysicalDeviceMemoryProperties2(m_handle, &m_memoryProperties);
}

std::vector<std::string> createDefaultDeviceExtensions()
{
    return {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
    };
}
} // namespace crisp