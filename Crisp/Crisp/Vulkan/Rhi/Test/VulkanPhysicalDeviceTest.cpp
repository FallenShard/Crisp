#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

#include <typeindex>

namespace crisp {
namespace {
using VulkanPhysicalDeviceWithSurfaceTest = VulkanTestWithSurface;
using VulkanPhysicalDeviceTest = VulkanTest;

using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::SizeIs;

// TEST(VulkanPhysicalDeviceSelectionTest, DeviceSelection) {
//     const VulkanInstance instance(nullptr, {}, false);
//     EXPECT_THAT(instance.getSurface(), IsNull());
//     EXPECT_THAT(instance.getHandle(), Not(IsNull()));
//     EXPECT_THAT(selectPhysicalDevice(instance, {}), Not(HasError()));
// }

// TEST_F(VulkanPhysicalDeviceWithSurfaceTest, SurfaceCapabilities) {
//     const VulkanPhysicalDevice physicalDevice(selectPhysicalDevice(*instance_, {}).unwrap());
//     const auto queueFamilies = physicalDevice.queryQueueFamilyIndices(instance_->getSurface());
//     EXPECT_TRUE(queueFamilies.presentFamily.has_value());
//     EXPECT_TRUE(queueFamilies.graphicsFamily.has_value());
//     EXPECT_TRUE(queueFamilies.computeFamily.has_value());
//     EXPECT_TRUE(queueFamilies.transferFamily.has_value());

//     const auto surfaceSupport = physicalDevice.querySurfaceSupport(instance_->getSurface());
//     EXPECT_THAT(surfaceSupport.formats, Not(IsEmpty()));
//     EXPECT_THAT(surfaceSupport.presentModes, Not(IsEmpty()));
// }

// TEST_F(VulkanPhysicalDeviceWithSurfaceTest, CreateDefaultQueueConfiguration) {
//     const VulkanPhysicalDevice physicalDevice(selectPhysicalDevice(*instance_, {}).unwrap());
//     const auto queueConfig = createDefaultQueueConfiguration(*instance_, physicalDevice);
//     EXPECT_THAT(queueConfig.createInfos, SizeIs(3));
//     EXPECT_THAT(queueConfig.priorities, SizeIs(Ge(3)));
//     EXPECT_THAT(queueConfig.identifiers, SizeIs(3));
//     EXPECT_THAT(queueConfig.types, SizeIs(3));
// }

// TEST_F(VulkanPhysicalDeviceWithSurfaceTest, CreateLogicalDevice) {
//     const VulkanPhysicalDevice physicalDevice(selectPhysicalDevice(*instance_, {}).unwrap());

//     const VkDevice device =
//         createLogicalDeviceHandle(physicalDevice, createDefaultQueueConfiguration(*instance_, physicalDevice));
//     EXPECT_THAT(device, Not(IsNull()));
//     vkDestroyDevice(device, nullptr);
// }

// TEST_F(VulkanPhysicalDeviceTest, Features) {
//     EXPECT_TRUE(physicalDevice_->getFeatures().tessellationShader);
// }

// TEST_F(VulkanPhysicalDeviceTest, FindDepthFormat) {
//     ASSERT_EQ(physicalDevice_->findSupportedDepthFormat().unwrap(), VK_FORMAT_D32_SFLOAT);
// }

// TEST_F(VulkanPhysicalDeviceTest, MemoryTypes) {
//     const auto device = device_->getHandle();
//     EXPECT_TRUE(physicalDevice_->findStagingBufferMemoryType(device).hasValue());
//     EXPECT_TRUE(physicalDevice_->findDeviceBufferMemoryType(device).hasValue());
//     EXPECT_TRUE(physicalDevice_->findDeviceImageMemoryType(device).hasValue());

//     EXPECT_EQ(
//         physicalDevice_->findStagingBufferMemoryType(device).unwrap(),
//         physicalDevice_->findMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT).unwrap());
// }

// TEST_F(VulkanPhysicalDeviceTest, FormatProperties) {
//     const auto props = physicalDevice_->getFormatProperties(VK_FORMAT_D32_SFLOAT);
//     EXPECT_TRUE(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
//     EXPECT_TRUE(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
// }

class VulkanFeatureChain {
public:
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan14Features features14{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageableDeviceLocalMemoryFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT};
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT};
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};

    void reset() {
        features.pNext = nullptr;
        isLinked.clear();
    }

    template <typename FeatureStruct>
    void link(FeatureStruct& featureStruct) {
        if (isLinked.contains(typeid(FeatureStruct))) {
            return;
        }
        featureStruct.pNext = features.pNext;
        features.pNext = &featureStruct;

        isLinked.emplace(typeid(FeatureStruct));
    }

    FlatHashSet<std::type_index> isLinked;
};

struct DeviceFeatureRequest {
    std::string extensionName;
    bool isOptional{false};
    std::function<bool(const VkPhysicalDevice)> supportCallback = [](const VkPhysicalDevice) { return true; };
    std::function<void(VulkanFeatureChain&)> featureChainCallback = [](VulkanFeatureChain&) {};

    std::vector<DeviceFeatureRequest> dependencies;

    void emplaceExtensions(FlatHashSet<std::string>& supportedExtensions) const {
        if (!extensionName.empty()) {
            supportedExtensions.emplace(extensionName);
        }
        for (const auto& dep : dependencies) {
            dep.emplaceExtensions(supportedExtensions);
        }
    }

    void emplaceFeatures(VulkanFeatureChain& featureChain) const {
        featureChainCallback(featureChain);
        for (const auto& dep : dependencies) {
            dep.emplaceFeatures(featureChain);
        }
    }
};

template <typename FeatureStruct>
consteval VkStructureType getFeatureStructureType() {
    if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceRayTracingPipelineFeaturesKHR>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceMeshShaderFeaturesEXT>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceFragmentDensityMapFeaturesEXT>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceAccelerationStructureFeaturesKHR>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT;
    } else {
        static_assert(sizeof(FeatureStruct) == 0, "Unknown feature structure type.");
    }
}

template <typename FeatureStruct>
FeatureStruct queryFeatures(const VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceFeatures2 baseFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

    FeatureStruct features{.sType = getFeatureStructureType<FeatureStruct>()};
    baseFeatures.pNext = &features;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &baseFeatures);
    return features;
}

std::vector<DeviceFeatureRequest> kRequestedFeatures = {
    DeviceFeatureRequest{
        .featureChainCallback = [](VulkanFeatureChain& featureChain) { featureChain.link(featureChain.features11); },
    },
    DeviceFeatureRequest{
        .featureChainCallback = [](VulkanFeatureChain& featureChain) { featureChain.link(featureChain.features12); },
    },
    DeviceFeatureRequest{
        .featureChainCallback = [](VulkanFeatureChain& featureChain) { featureChain.link(featureChain.features13); },
    },
    DeviceFeatureRequest{
        .featureChainCallback = [](VulkanFeatureChain& featureChain) { featureChain.link(featureChain.features14); },
    },
    DeviceFeatureRequest{
        .extensionName = VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    },
    DeviceFeatureRequest{
        .extensionName = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        .supportCallback =
            [](const VkPhysicalDevice physicalDevice) {
                const auto rayTracingFeatures =
                    queryFeatures<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(physicalDevice);
                return rayTracingFeatures.rayTracingPipeline == VK_TRUE;
            },
        .featureChainCallback =
            [](VulkanFeatureChain& featureChain) { featureChain.link(featureChain.rayTracingFeatures); },
        .dependencies =
            {
                DeviceFeatureRequest{
                    .extensionName = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                    .dependencies =
                        {
                            DeviceFeatureRequest{
                                .extensionName = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                            },
                        }},
            },
    },
    DeviceFeatureRequest{
        .extensionName = VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,
        .isOptional = true,
        .featureChainCallback =
            [](VulkanFeatureChain& featureChain) { featureChain.link(featureChain.fragmentDensityMapFeatures); },
    },
    {
        .extensionName = VK_EXT_MESH_SHADER_EXTENSION_NAME,
        .isOptional = true,
        .featureChainCallback =
            [](VulkanFeatureChain& featureChain) {
                featureChain.link(featureChain.meshShaderFeatures);
                featureChain.link(featureChain.fragmentShadingRateFeatures);
            },
        .dependencies =
            {
                DeviceFeatureRequest{
                    .extensionName = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
                },
            },
    },
};

bool checkSupport(
    const VkPhysicalDevice physicalDevice,
    const std::span<const DeviceFeatureRequest> featureRequests,
    VulkanFeatureChain& supportedFeatures,
    FlatHashSet<std::string>& supportedExtensions) {
    const auto availableExtensions = querySupportedExtensions(physicalDevice);
    FlatHashSet<std::string> availableExtensionsSet;
    for (const auto& extProp : availableExtensions) {
        availableExtensionsSet.emplace(extProp.extensionName);
    }

    supportedFeatures.reset();
    supportedExtensions.clear();
    for (const auto& featureRequest : featureRequests) {
        const bool extensionSupported =
            featureRequest.extensionName.empty() || availableExtensionsSet.contains(featureRequest.extensionName);
        const bool featureSupported =
            featureRequest.supportCallback ? featureRequest.supportCallback(physicalDevice) : true;

        if (!extensionSupported || !featureSupported) {
            if (featureRequest.isOptional) {
                spdlog::warn("Optional extension {} is not supported.", featureRequest.extensionName);
            } else {
                spdlog::error("Required extension {} is not supported.", featureRequest.extensionName);
                return false;
            }
        } else {
            featureRequest.emplaceFeatures(supportedFeatures);
            featureRequest.emplaceExtensions(supportedExtensions);
        }
    }

    return true;
}

TEST(VulkanExtensionTest, RequireFeatures) {
    glfwInit();
    {
        const Window window(glm::ivec2{0, 0}, glm::ivec2{200, 200}, "unit_test", WindowVisibility::Hidden);

        const VulkanInstance instance(
            window.createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanInstanceExtensions(), true);
        EXPECT_THAT(instance.getSurface(), Not(IsNull()));
        EXPECT_THAT(instance.getHandle(), Not(IsNull()));

        const auto physicalDevices = enumeratePhysicalDevices(instance);

        VkPhysicalDevice selectedPhysicalDevice{VK_NULL_HANDLE};
        FlatHashSet<std::string> supportedExtensions;
        VulkanFeatureChain supportedFeatures{};
        for (const auto physicalDevice : physicalDevices) {
            VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            vkGetPhysicalDeviceProperties2(physicalDevice, &properties);
            spdlog::info("Checking {}", properties.properties.deviceName);
            if (!checkSupport(physicalDevice, kRequestedFeatures, supportedFeatures, supportedExtensions)) {
                spdlog::error("Device {} does not support all requested features.", properties.properties.deviceName);
            } else {
                selectedPhysicalDevice = physicalDevice;
                break;
            }
        }
        ASSERT_NE(selectedPhysicalDevice, VK_NULL_HANDLE);

        vkGetPhysicalDeviceFeatures2(selectedPhysicalDevice, &supportedFeatures.features);
        const std::vector<const char*> enabledExtensions(
            std::ranges::transform_view(supportedExtensions, [](const auto& str) { return str.c_str(); }) |
            std::ranges::to<std::vector>());
        spdlog::info("Enabled extensions: {}", enabledExtensions.size());
        for (const auto& ext : enabledExtensions) {
            spdlog::info(" - {}", ext);
        }

        VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceCreateInfo.pNext = &supportedFeatures.features;

        VkDeviceQueueCreateInfo deviceQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        deviceQueueCreateInfo.pNext = nullptr;
        deviceQueueCreateInfo.flags = 0;
        deviceQueueCreateInfo.queueFamilyIndex = 0;
        deviceQueueCreateInfo.queueCount = 1;
        const float priority = 1.0f;
        deviceQueueCreateInfo.pQueuePriorities = &priority;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;

        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();

        VkDevice device(VK_NULL_HANDLE);
        vkCreateDevice(selectedPhysicalDevice, &deviceCreateInfo, nullptr, &device);
        volkLoadDevice(device);
        ASSERT_NE(device, VK_NULL_HANDLE);
        vkDestroyDevice(device, nullptr);
    }

    glfwTerminate();
}

} // namespace
} // namespace crisp