
#include <Crisp/Vulkan/Rhi/VulkanPhysicalDevice.hpp>

#include <algorithm>

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

void logRequestedExtensions(const VulkanDeviceFeatureRequest& request) {
    CRISP_LOGI(" - [{}] {}", request.isRequired ? "Required" : "Optional", request.getLoggingName());
    for (const auto& dep : request.prerequisites) {
        logRequestedExtensions(dep);
    }
}

std::string formatApiVersion(const uint32_t apiVersion) {
    return fmt::format(
        "{}.{}.{}", VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion), VK_VERSION_PATCH(apiVersion));
}

void logSelectedDevice(const VulkanPhysicalDevice& physicalDevice) {
    CRISP_LOGI(
        "Selected device: {}, type: {}",
        physicalDevice.getProperties().deviceName,
        getDeviceTypeString(physicalDevice.getProperties().deviceType));
    CRISP_LOGI(
        " - API version:    {} (requested {}, device supports {})",
        formatApiVersion(physicalDevice.getApiVersion()),
        formatApiVersion(physicalDevice.getRequestedApiVersion()),
        formatApiVersion(physicalDevice.getSupportedApiVersion()));
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
        CRISP_LOGI(" - Driver version: {}", driverVersion);
    }
}

} // namespace

VulkanPhysicalDevice::VulkanPhysicalDevice(const VkPhysicalDevice handle, const uint32_t requestedApiVersion)
    : m_handle(handle)
    , m_requestedApiVersion(requestedApiVersion)
    , m_capabilities(std::make_unique<VulkanPhysicalDeviceProperties>()) {
    const auto availableExtensions = querySupportedExtensions(m_handle);
    for (const auto& extProp : availableExtensions) {
        m_extensions.emplace(extProp.extensionName);
    }

    append(m_capabilities->properties, m_capabilities->properties11);
    append(m_capabilities->properties, m_capabilities->properties12);
    append(m_capabilities->properties, m_capabilities->properties13);
    append(m_capabilities->properties, m_capabilities->properties14);
    append(m_capabilities->properties, m_capabilities->rayTracingProperties);
    append(m_capabilities->properties, m_capabilities->meshShaderProperties);
    vkGetPhysicalDeviceProperties2(m_handle, &m_capabilities->properties);

    vkGetPhysicalDeviceMemoryProperties2(m_handle, &m_capabilities->memoryProperties);
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

        if (queueFamily.queueCount > 0 && surface && supportsPresentation(i, surface)) {
            indices.present = i;
        }

        if (indices.isComplete(surface != VK_NULL_HANDLE)) {
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
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer dummyBuffer(VK_NULL_HANDLE);
    vkCreateBuffer(device, &bufferInfo, nullptr, &dummyBuffer);
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, dummyBuffer, &memRequirements);
    vkDestroyBuffer(device, dummyBuffer, nullptr);

    return findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
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
    return m_extensions;
}

std::vector<VkPhysicalDevice> enumeratePhysicalDevices(const VulkanInstance& instance) {
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance.getHandle(), &deviceCount, nullptr));
    CRISP_CHECK_GT(deviceCount, 0, "Vulkan found no physical devices.");

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

std::string VulkanDeviceFeatureRequest::getLoggingName() const {
    return symbolicName.empty() ? extensionName : symbolicName;
}

bool VulkanDeviceFeatureRequest::isSupported(const VulkanPhysicalDevice& physicalDevice) const {
    if (physicalDevice.getApiVersion() < minApiVersion) {
        return false;
    }

    const bool extensionSupported =
        extensionName.empty() || physicalDevice.getAvailableExtensions().contains(extensionName);
    if (!extensionSupported) {
        return false;
    }

    const bool featureSupported = isSupportedFunc(physicalDevice);
    if (!featureSupported) {
        return false;
    }

    return std::ranges::all_of(prerequisites, [&physicalDevice](const auto& prereq) {
        return prereq.isSupported(physicalDevice);
    });
}

void VulkanDeviceFeatureRequest::link(
    FlatStringHashSet& extensionsToEnable, VulkanDeviceFeatureChain& featureChainToEnable) const {
    if (!extensionName.empty()) {
        extensionsToEnable.emplace(extensionName);
    }
    linkFunc(featureChainToEnable);
    for (const auto& prereq : prerequisites) {
        prereq.link(extensionsToEnable, featureChainToEnable);
    }
}

void VulkanDeviceFeatureRequest::set(VulkanDeviceFeatures& features) const {
    setFunc(features);
    for (const auto& prereq : prerequisites) {
        prereq.set(features);
    }
}

std::vector<VulkanDeviceFeatureRequest> createDefaultFeatureRequests() {
    return {
        VulkanDeviceFeatureRequest{
            .symbolicName = "Core 1.0 Features",
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    const auto& core = physicalDevice.queryFeatures();
                    return core.samplerAnisotropy && core.fillModeNonSolid && core.geometryShader &&
                           core.tessellationShader;
                },
            .linkFunc =
                [](VulkanDeviceFeatureChain& featureChain) {
                    auto& core = featureChain.features.features;
                    core.samplerAnisotropy = VK_TRUE;
                    core.fillModeNonSolid = VK_TRUE;
                    core.geometryShader = VK_TRUE;
                    core.tessellationShader = VK_TRUE;
                },
        },
        VulkanDeviceFeatureRequest{
            .symbolicName = "Core 1.1 Features",
            .minApiVersion = VK_API_VERSION_1_1,
            .linkFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features11); },
        },
        VulkanDeviceFeatureRequest{
            .symbolicName = "Core 1.2 Features",
            .minApiVersion = VK_API_VERSION_1_2,
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    const auto f12 = physicalDevice.queryFeatures<VkPhysicalDeviceVulkan12Features>();
                    return f12.bufferDeviceAddress && f12.hostQueryReset && f12.timelineSemaphore &&
                           f12.scalarBlockLayout && f12.descriptorIndexing && f12.runtimeDescriptorArray &&
                           f12.descriptorBindingPartiallyBound && f12.descriptorBindingVariableDescriptorCount &&
                           f12.descriptorBindingUniformBufferUpdateAfterBind;
                },
            .linkFunc =
                [](VulkanDeviceFeatureChain& featureChain) {
                    auto& f12 = featureChain.link(featureChain.features12);
                    f12.bufferDeviceAddress = VK_TRUE;
                    f12.hostQueryReset = VK_TRUE;
                    f12.timelineSemaphore = VK_TRUE;
                    f12.scalarBlockLayout = VK_TRUE;
                    f12.descriptorIndexing = VK_TRUE;
                    f12.runtimeDescriptorArray = VK_TRUE;
                    f12.descriptorBindingPartiallyBound = VK_TRUE;
                    f12.descriptorBindingVariableDescriptorCount = VK_TRUE;
                    f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
                    f12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
                    f12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
                    f12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
                },
        },
        VulkanDeviceFeatureRequest{
            .symbolicName = "Core 1.3 Features",
            .minApiVersion = VK_API_VERSION_1_3,
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    const auto f13 = physicalDevice.queryFeatures<VkPhysicalDeviceVulkan13Features>();
                    return f13.synchronization2 && f13.maintenance4;
                },
            .linkFunc =
                [](VulkanDeviceFeatureChain& featureChain) {
                    auto& f13 = featureChain.link(featureChain.features13);
                    f13.synchronization2 = VK_TRUE;
                    f13.maintenance4 = VK_TRUE;
                },
        },
        VulkanDeviceFeatureRequest{
            // Optional because it enables nothing yet - a 1.3 device should run, not be rejected over an empty
            // feature struct. Make it required once something here is actually used.
            .symbolicName = "Core 1.4 Features",
            .minApiVersion = VK_API_VERSION_1_4,
            .isRequired = false,
            .linkFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features14); },
        },
        VulkanDeviceFeatureRequest{
            .extensionName = VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        }};
}

void addPageableMemoryFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests) {
    featureRequests.emplace_back(
        VulkanDeviceFeatureRequest{
            .extensionName = VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
            .isRequired = false,
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    const auto features =
                        physicalDevice.queryFeatures<VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT>();
                    return features.pageableDeviceLocalMemory == VK_TRUE;
                },
            .linkFunc =
                [](VulkanDeviceFeatureChain& featureChain) {
                    featureChain.link(featureChain.pageableDeviceLocalMemoryFeatures);
                    featureChain.pageableDeviceLocalMemoryFeatures.pageableDeviceLocalMemory = VK_TRUE;
                },
            .setFunc = [](VulkanDeviceFeatures& features) { features.pageableMemory = true; },
            .prerequisites =
                {
                    VulkanDeviceFeatureRequest{
                        .extensionName = VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
                    },
                }});
}

void addRayTracingFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests) {
    featureRequests.emplace_back(
        VulkanDeviceFeatureRequest{
            .extensionName = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            .isRequired = false,
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    const auto features = physicalDevice.queryFeatures<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>();
                    return features.rayTracingPipeline == VK_TRUE;
                },
            .linkFunc =
                [](VulkanDeviceFeatureChain& featureChain) {
                    featureChain.link(featureChain.rayTracingFeatures);
                    featureChain.rayTracingFeatures.rayTracingPipeline = VK_TRUE;
                },
            .setFunc = [](VulkanDeviceFeatures& features) { features.rayTracing = true; },
            .prerequisites =
                {
                    VulkanDeviceFeatureRequest{
                        .extensionName = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                        .isSupportedFunc =
                            [](const VulkanPhysicalDevice& physicalDevice) {
                                const auto features =
                                    physicalDevice.queryFeatures<VkPhysicalDeviceAccelerationStructureFeaturesKHR>();
                                return features.accelerationStructure == VK_TRUE;
                            },
                        .linkFunc =
                            [](VulkanDeviceFeatureChain& featureChain) {
                                featureChain.link(featureChain.accelerationStructureFeatures);
                                featureChain.accelerationStructureFeatures.accelerationStructure = VK_TRUE;
                            },
                        .setFunc = [](VulkanDeviceFeatures& features) { features.rayTracing = true; },
                        .prerequisites =
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
            .isRequired = false,
            .isSupportedFunc =
                [](const VulkanPhysicalDevice& physicalDevice) {
                    const auto features = physicalDevice.queryFeatures<VkPhysicalDeviceMeshShaderFeaturesEXT>();
                    return features.meshShader == VK_TRUE;
                },
            .linkFunc =
                [](VulkanDeviceFeatureChain& featureChain) {
                    featureChain.link(featureChain.meshShaderFeatures);
                    featureChain.meshShaderFeatures.meshShader = VK_TRUE;
                },
            .setFunc = [](VulkanDeviceFeatures& features) { features.meshShading = true; },
        });
}

bool isPhysicalDeviceSuitable(
    const VulkanPhysicalDevice& physicalDevice,
    const VulkanInstance& instance,
    const std::span<const VulkanDeviceFeatureRequest> featureRequests,
    VulkanDeviceFeatureChain& featureChainToEnable,
    FlatStringHashSet& extensionsToEnable,
    VulkanDeviceFeatures& supportedFeatures) {
    const auto queueFamilySupport = physicalDevice.queryQueueFamilySupport(instance.getSurface());
    if (!queueFamilySupport.isComplete(/*requirePresentation=*/instance.getSurface() != VK_NULL_HANDLE)) {
        return false;
    }
    if (instance.getSurface() != VK_NULL_HANDLE) {
        const SurfaceSupport surfaceSupport = physicalDevice.querySurfaceSupport(instance.getSurface());
        if (surfaceSupport.formats.empty() || surfaceSupport.presentModes.empty()) {
            return false;
        }
    }
    if (physicalDevice.getProperties().deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        return false;
    }

    for (const auto& featureRequest : featureRequests) {
        if (!featureRequest.isSupported(physicalDevice)) {
            if (featureRequest.isRequired) {
                spdlog::error("Required extension {} is not supported.", featureRequest.getLoggingName());
                return false;
            }
            spdlog::warn("Optional extension {} is not supported.", featureRequest.getLoggingName());
            continue;
        }
        featureRequest.link(extensionsToEnable, featureChainToEnable);
        featureRequest.set(supportedFeatures);
    }

    return true;
}

Result<VulkanPhysicalDevice> selectPhysicalDevice(
    const VulkanInstance& instance,
    const std::span<const VulkanDeviceFeatureRequest> featureRequests,
    VulkanDeviceFeatureChain& featureChainToEnable,
    FlatStringHashSet& extensionsToEnable,
    VulkanDeviceFeatures& supportedFeatures) {
    const auto devices = enumeratePhysicalDevices(instance);

    if (!featureRequests.empty()) {
        CRISP_LOGI("Requesting {} device features with the following extensions:", featureRequests.size());
        for (const auto& featureRequest : featureRequests) {
            logRequestedExtensions(featureRequest);
        }
    } else {
        CRISP_LOGI("No device feature requests specified.");
    }

    for (const auto deviceHandle : devices) {
        VulkanPhysicalDevice physicalDevice(deviceHandle, instance.getApiVersion());

        featureChainToEnable.reset();
        extensionsToEnable.clear();
        supportedFeatures = {};
        if (isPhysicalDeviceSuitable(
                physicalDevice, instance, featureRequests, featureChainToEnable, extensionsToEnable, supportedFeatures)) {
            logSelectedDevice(physicalDevice);
            return physicalDevice;
        }
    }

    return resultError("Failed to find a suitable physical device!");
}
} // namespace crisp