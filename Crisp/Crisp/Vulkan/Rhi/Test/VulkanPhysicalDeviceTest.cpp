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

TEST(VulkanPhysicalDeviceSelectionTest, DeviceSelection) {
    const VulkanInstance instance(nullptr, {}, false);
    EXPECT_THAT(instance.getSurface(), IsNull());
    EXPECT_THAT(instance.getHandle(), Not(IsNull()));
    VulkanDeviceFeatureChain featureChain{};
    FlatStringHashSet extensions{};
    EXPECT_THAT(selectPhysicalDevice(instance, {}, featureChain, extensions), Not(HasError()));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, SurfaceCapabilities) {
    VulkanDeviceFeatureChain featureChain{};
    FlatStringHashSet extensions{};
    const VulkanPhysicalDevice physicalDevice(selectPhysicalDevice(*instance_, {}, featureChain, extensions).unwrap());
    const auto queueFamilies = physicalDevice.queryQueueFamilySupport(instance_->getSurface());
    EXPECT_TRUE(queueFamilies.isComplete());

    const auto surfaceSupport = physicalDevice.querySurfaceSupport(instance_->getSurface());
    EXPECT_THAT(surfaceSupport.formats, Not(IsEmpty()));
    EXPECT_THAT(surfaceSupport.presentModes, Not(IsEmpty()));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, CreateDefaultQueueConfiguration) {
    VulkanDeviceFeatureChain featureChain{};
    FlatStringHashSet extensions{};
    const VulkanPhysicalDevice physicalDevice(selectPhysicalDevice(*instance_, {}, featureChain, extensions).unwrap());
    const auto queueConfig = createDefaultQueueConfiguration(*instance_, physicalDevice);
    EXPECT_THAT(queueConfig.createInfos, SizeIs(3));
    EXPECT_THAT(queueConfig.priorities, SizeIs(Ge(3)));
    EXPECT_THAT(queueConfig.identifiers, SizeIs(3));
    EXPECT_THAT(queueConfig.types, SizeIs(3));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, CreateLogicalDevice) {
    VulkanDeviceConfiguration deviceConfig{};
    const VulkanPhysicalDevice physicalDevice(
        selectPhysicalDevice(*instance_, {}, *deviceConfig.featureChain, deviceConfig.extensions).unwrap());
    deviceConfig.queueConfig = createDefaultQueueConfiguration(*instance_, physicalDevice);

    const VkDevice device = createLogicalDeviceHandle(physicalDevice, deviceConfig);
    EXPECT_THAT(device, Not(IsNull()));
    vkDestroyDevice(device, nullptr);
}

TEST_F(VulkanPhysicalDeviceTest, Features) {
    EXPECT_TRUE(physicalDevice_->getCapabilities().features.features.features.tessellationShader);
}

TEST_F(VulkanPhysicalDeviceTest, FindDepthFormat) {
    ASSERT_EQ(physicalDevice_->findSupportedDepthFormat().unwrap(), VK_FORMAT_D32_SFLOAT);
}

TEST_F(VulkanPhysicalDeviceTest, MemoryTypes) {
    const auto device = device_->getHandle();
    EXPECT_TRUE(physicalDevice_->findStagingBufferMemoryType(device).hasValue());
    EXPECT_TRUE(physicalDevice_->findDeviceBufferMemoryType(device).hasValue());
    EXPECT_TRUE(physicalDevice_->findDeviceImageMemoryType(device).hasValue());

    EXPECT_EQ(
        physicalDevice_->findStagingBufferMemoryType(device).unwrap(),
        physicalDevice_->findMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT).unwrap());
}

TEST_F(VulkanPhysicalDeviceTest, FormatProperties) {
    const auto props = physicalDevice_->getFormatProperties(VK_FORMAT_D32_SFLOAT);
    EXPECT_TRUE(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    EXPECT_TRUE(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

template <typename FeatureStruct, VkStructureType StructureType>
FeatureStruct queryFeatures(const VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceFeatures2 baseFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

    FeatureStruct features{.sType = StructureType};
    baseFeatures.pNext = &features;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &baseFeatures);
    return features;
}

std::vector<VulkanDeviceFeatureRequest> kRequestedFeatures = {
    VulkanDeviceFeatureRequest{
        .addFeatureFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features11); },
    },
    VulkanDeviceFeatureRequest{
        .addFeatureFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features12); },
    },
    VulkanDeviceFeatureRequest{
        .addFeatureFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features13); },
    },
    VulkanDeviceFeatureRequest{
        .addFeatureFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features14); },
    },
    VulkanDeviceFeatureRequest{
        .extensionName = VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    },
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
    },
    VulkanDeviceFeatureRequest{
        .extensionName = VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,
        .isOptional = true,
        .isSupportedFunc =
            [](const VulkanPhysicalDevice& physicalDevice) {
                const auto features = queryFeatures<
                    VkPhysicalDeviceFragmentDensityMapFeaturesEXT,
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT>(physicalDevice.getHandle());
                return features.fragmentDensityMap == VK_TRUE;
            },
        .addFeatureFunc =
            [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.fragmentDensityMapFeatures); },
    },
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
    },
};

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
        FlatStringHashSet supportedExtensions;
        VulkanDeviceFeatureChain supportedFeatures{};
        for (const auto physicalDevice : physicalDevices) {
            const VulkanPhysicalDevice device(physicalDevice);

            VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            vkGetPhysicalDeviceProperties2(physicalDevice, &properties);
            spdlog::info("Checking {}", properties.properties.deviceName);
            supportedFeatures.reset();
            supportedExtensions.clear();
            if (!isPhysicalDeviceSuitable(device, instance, kRequestedFeatures, supportedFeatures, supportedExtensions)) {
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