
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

VulkanPhysicalDevice::VulkanPhysicalDevice(const VkPhysicalDevice handle)
    : m_handle(handle)
    , m_capabilities(std::make_unique<VulkanPhysicalDeviceCapabilities>()) {
    m_capabilities->features.link(m_capabilities->features.features11);
    m_capabilities->features.link(m_capabilities->features.features12);
    m_capabilities->features.link(m_capabilities->features.features13);
    m_capabilities->features.link(m_capabilities->features.features14);
    m_capabilities->features.link(m_capabilities->features.accelerationStructureFeatures);
    m_capabilities->features.link(m_capabilities->features.rayTracingFeatures);
    m_capabilities->features.link(m_capabilities->features.pageableDeviceLocalMemoryFeatures);
    m_capabilities->features.link(m_capabilities->features.meshShaderFeatures);
    m_capabilities->features.link(m_capabilities->features.fragmentShadingRateFeatures);
    vkGetPhysicalDeviceFeatures2(m_handle, &m_capabilities->features.features);

    append(m_capabilities->properties, m_capabilities->properties11);
    append(m_capabilities->properties, m_capabilities->properties12);
    append(m_capabilities->properties, m_capabilities->properties13);
    append(m_capabilities->properties, m_capabilities->properties14);
    append(m_capabilities->properties, m_capabilities->rayTracingProperties);
    append(m_capabilities->properties, m_capabilities->meshShaderProperties);
    vkGetPhysicalDeviceProperties2(m_handle, &m_capabilities->properties);

    vkGetPhysicalDeviceMemoryProperties2(m_handle, &m_capabilities->memoryProperties);

    const auto availableExtensions = querySupportedExtensions(m_handle);
    for (const auto& extProp : availableExtensions) {
        m_capabilities->extensions.emplace(extProp.extensionName);
    }
}

bool VulkanPhysicalDevice::supportsPresentation(const uint32_t queueFamilyIndex, const VkSurfaceKHR surface) const {
    if (surface == VK_NULL_HANDLE) {
        return false;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_handle, queueFamilyIndex, surface, &presentSupport);
    return static_cast<bool>(presentSupport);
}

QueueFamilySupport VulkanPhysicalDevice::queryQueueFamilySupport(const VkSurfaceKHR surface) const {
    QueueFamilySupport indices;

    const auto queueFamilies = queryQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& queueFamily = queueFamilies[i];

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.compute = i;
        }

        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            indices.transfer = i;
        }

        if (queueFamily.queueCount > 0 && supportsPresentation(i, surface)) {
            indices.present = i;
        }

        if (indices.isComplete()) {
            return indices;
        }
    }

    return indices;
}

SurfaceSupport VulkanPhysicalDevice::querySurfaceSupport(const VkSurfaceKHR surface) const {
    SurfaceSupport surfaceSupport{};
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
    const auto availableExtensions = querySupportedExtensions(m_handle);

    FlatHashSet<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : availableExtensions) {
        requiredExtensions.erase(ext.extensionName); // NOLINT
    }

    return requiredExtensions.empty();
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

const FlatStringHashSet& VulkanPhysicalDevice::getAvailableExtensions() const {
    return m_capabilities->extensions;
}

std::vector<VkPhysicalDevice> enumeratePhysicalDevices(const VulkanInstance& instance) {
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance.getHandle(), &deviceCount, nullptr));
    CRISP_CHECK_GE(deviceCount, 0, "Vulkan found no physical devices.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance.getHandle(), &deviceCount, devices.data()));
    return devices;
}

std::vector<VkExtensionProperties> querySupportedExtensions(const VkPhysicalDevice physicalDevice) {
    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> extensions(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data()));

    return extensions;
}

void VulkanDeviceFeatureRequest::appendTo(
    FlatStringHashSet& extensionsToEnable, VulkanDeviceFeatureChain& featureChainToEnable) const {
    if (!extensionName.empty()) {
        extensionsToEnable.emplace(extensionName);
    }
    addFeatureFunc(featureChainToEnable);
    for (const auto& dep : dependencies) {
        dep.appendTo(extensionsToEnable, featureChainToEnable);
    }
}

std::vector<VulkanDeviceFeatureRequest> createDefaultFeatureRequests() {
    return {
        VulkanDeviceFeatureRequest{
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    return physicalDevice.getProperties().apiVersion >= VK_MAKE_VERSION(1, 1, 0);
                },
            .addFeatureFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features11); },
        },
        VulkanDeviceFeatureRequest{
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    return physicalDevice.getProperties().apiVersion >= VK_MAKE_VERSION(1, 2, 0);
                },
            .addFeatureFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features12); },
        },
        VulkanDeviceFeatureRequest{
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    return physicalDevice.getProperties().apiVersion >= VK_MAKE_VERSION(1, 3, 0);
                },
            .addFeatureFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features13); },
        },
        VulkanDeviceFeatureRequest{
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    return physicalDevice.getProperties().apiVersion >= VK_MAKE_VERSION(1, 4, 0);
                },
            .addFeatureFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features14); },
        },
        VulkanDeviceFeatureRequest{
            .extensionName = VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        }};
}

void addPageableMemoryFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests) {
    featureRequests.emplace_back(
        VulkanDeviceFeatureRequest{
            .extensionName = VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
            .dependencies = {
                VulkanDeviceFeatureRequest{
                    .extensionName = VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
                },
            }});
}

void addRayTracingFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests) {
    featureRequests.emplace_back(
        VulkanDeviceFeatureRequest{
            .extensionName = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    return physicalDevice.getCapabilities().features.rayTracingFeatures.rayTracingPipeline == VK_TRUE;
                },
            .addFeatureFunc =
                [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.rayTracingFeatures); },
            .dependencies =
                {
                    VulkanDeviceFeatureRequest{
                        .extensionName = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                        .addFeatureFunc =
                            [](VulkanDeviceFeatureChain& featureChain) {
                                featureChain.link(featureChain.accelerationStructureFeatures);
                            },
                        .dependencies =
                            {
                                VulkanDeviceFeatureRequest{
                                    .extensionName = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                },
                            }},
                },
        });
}

void addMeshShadingFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests) {
    featureRequests.emplace_back(
        VulkanDeviceFeatureRequest{
            .extensionName = VK_EXT_MESH_SHADER_EXTENSION_NAME,
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    return physicalDevice.getCapabilities().features.meshShaderFeatures.meshShader == VK_TRUE;
                },
            .addFeatureFunc =
                [](VulkanDeviceFeatureChain& featureChain) {
                    featureChain.link(featureChain.meshShaderFeatures);
                    featureChain.link(featureChain.fragmentShadingRateFeatures);
                },
            .dependencies =
                {
                    VulkanDeviceFeatureRequest{
                        .extensionName = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
                    },
                },
        });
}

bool isPhysicalDeviceSuitable(
    const VulkanPhysicalDevice& physicalDevice,
    const VulkanInstance& instance,
    const std::span<const VulkanDeviceFeatureRequest> featureRequests,
    VulkanDeviceFeatureChain& supportedFeatures,
    FlatStringHashSet& supportedExtensions) {
    const auto isFeatureRequestSupported = [&physicalDevice](const VulkanDeviceFeatureRequest& featureRequest) {
        const bool extensionSupported =
            featureRequest.extensionName.empty() ||
            physicalDevice.getAvailableExtensions().contains(featureRequest.extensionName);
        if (!extensionSupported) {
            return false;
        }

        return featureRequest.isSupportedFunc ? featureRequest.isSupportedFunc(physicalDevice) : true;
    };

    if (!physicalDevice.queryQueueFamilySupport(instance.getSurface()).isComplete()) {
        return false;
    }
    if (physicalDevice.getProperties().deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        return false;
    }

    const SurfaceSupport surfaceSupport = physicalDevice.querySurfaceSupport(instance.getSurface());
    if (surfaceSupport.formats.empty() || surfaceSupport.presentModes.empty()) {
        return false;
    }

    for (const auto& featureRequest : featureRequests) {
        const bool isSupported = isFeatureRequestSupported(featureRequest);

        if (isSupported) {
            featureRequest.appendTo(supportedExtensions, supportedFeatures);
        } else {
            if (featureRequest.isOptional) {
                spdlog::warn("Optional extension {} is not supported.", featureRequest.extensionName);
            } else {
                spdlog::error("Required extension {} is not supported.", featureRequest.extensionName);
                break; // Break out of the loop if a required feature is not supported.
            }
        }
    }

    return true;
}

Result<VulkanPhysicalDevice> selectPhysicalDevice(
    const VulkanInstance& instance,
    const std::span<const VulkanDeviceFeatureRequest> featureRequests,
    VulkanDeviceFeatureChain& supportedFeatures,
    FlatStringHashSet& supportedExtensions) {
    const auto devices = enumeratePhysicalDevices(instance);

    if (instance.getSurface() == VK_NULL_HANDLE) {
        return VulkanPhysicalDevice(devices.front());
    }

    if (!featureRequests.empty()) {
        const auto logExtensions = [&](this const auto& self, const VulkanDeviceFeatureRequest& request) -> void {
            if (!request.extensionName.empty()) {
                CRISP_LOGI(" - [{}] {}", request.isOptional ? "Optional" : "Required", request.extensionName);
            }
            for (const auto& dep : request.dependencies) {
                self(dep);
            }
        };
        CRISP_LOGI("Requesting {} device features with the following extensions:", featureRequests.size());
        for (const auto& featureRequest : featureRequests) {
            logExtensions(featureRequest);
        }
    }

    // const DeviceRequirements requirements{
    //     .rayTracing = requestedExtensions.contains(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME),
    //     .pageableMemory = requestedExtensions.contains(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME),
    //     .meshShading = requestedExtensions.contains(VK_EXT_MESH_SHADER_EXTENSION_NAME),
    // };
    for (const auto deviceHandle : devices) {
        VulkanPhysicalDevice physicalDevice(deviceHandle);

        supportedFeatures.reset();
        supportedExtensions.clear();
        if (isPhysicalDeviceSuitable(physicalDevice, instance, featureRequests, supportedFeatures, supportedExtensions)) {
            logSelectedDevice(physicalDevice);
            return physicalDevice;
        }
    }

    return resultError("Failed to find a suitable physical device!");
}
} // namespace crisp