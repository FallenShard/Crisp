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
    VulkanDeviceFeatures enabledFeatures{};
    EXPECT_THAT(selectPhysicalDevice(instance, {}, featureChain, extensions, enabledFeatures), Not(HasError()));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, SurfaceCapabilities) {
    VulkanDeviceFeatureChain featureChain{};
    FlatStringHashSet extensions{};
    VulkanDeviceFeatures enabledFeatures{};
    const VulkanPhysicalDevice physicalDevice(
        selectPhysicalDevice(*instance_, {}, featureChain, extensions, enabledFeatures).unwrap());
    const auto queueFamilies = physicalDevice.queryQueueFamilySupport(instance_->getSurface());
    EXPECT_TRUE(queueFamilies.isComplete(/*requirePresentation=*/true));

    const auto surfaceSupport = physicalDevice.querySurfaceSupport(instance_->getSurface());
    EXPECT_THAT(surfaceSupport.formats, Not(IsEmpty()));
    EXPECT_THAT(surfaceSupport.presentModes, Not(IsEmpty()));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, CreateDefaultQueueConfiguration) {
    VulkanDeviceFeatureChain featureChain{};
    FlatStringHashSet extensions{};
    VulkanDeviceFeatures enabledFeatures{};
    const VulkanPhysicalDevice physicalDevice(
        selectPhysicalDevice(*instance_, {}, featureChain, extensions, enabledFeatures).unwrap());
    const auto queueConfig = createDefaultQueueConfiguration(*instance_, physicalDevice);
    EXPECT_THAT(queueConfig.createInfos, SizeIs(3));
    EXPECT_THAT(queueConfig.priorities, SizeIs(Ge(3)));
    EXPECT_THAT(queueConfig.identifiers, SizeIs(3));
    EXPECT_THAT(queueConfig.types, SizeIs(3));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, CreateLogicalDevice) {
    VulkanDeviceConfiguration deviceConfig{};
    VulkanDeviceFeatures enabledFeatures{};
    const VulkanPhysicalDevice physicalDevice(
        selectPhysicalDevice(*instance_, {}, *deviceConfig.featureChain, deviceConfig.extensions, enabledFeatures)
            .unwrap());
    deviceConfig.queueConfig = createDefaultQueueConfiguration(*instance_, physicalDevice);

    const VkDevice device = createLogicalDeviceHandle(physicalDevice, deviceConfig);
    EXPECT_THAT(device, Not(IsNull()));
    vkDestroyDevice(device, nullptr);
}

TEST_F(VulkanPhysicalDeviceTest, Features) {
    EXPECT_TRUE(physicalDevice_->queryFeatures().tessellationShader);
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

std::vector<VulkanDeviceFeatureRequest> kRequestedFeatures = {
    VulkanDeviceFeatureRequest{
        .symbolicName = "Core 1.1 Features",
        .linkFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features11); },
    },
    VulkanDeviceFeatureRequest{
        .symbolicName = "Core 1.2 Features",
        .linkFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features12); },
    },
    VulkanDeviceFeatureRequest{
        .symbolicName = "Core 1.3 Features",
        .linkFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features13); },
    },
    VulkanDeviceFeatureRequest{
        .symbolicName = "Core 1.4 Features",
        .linkFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.features14); },
    },
    VulkanDeviceFeatureRequest{
        .extensionName = VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    },
    VulkanDeviceFeatureRequest{
        .extensionName = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        .isSupportedFunc =
            [](const VulkanPhysicalDevice& physicalDevice) {
                return physicalDevice.queryFeatures<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline ==
                       VK_TRUE;
            },
        .linkFunc = [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.rayTracingFeatures); },
        .prerequisites =
            {
                VulkanDeviceFeatureRequest{
                    .extensionName = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                    .linkFunc = [](VulkanDeviceFeatureChain&
                                       featureChain) { featureChain.link(featureChain.accelerationStructureFeatures); },
                    .prerequisites =
                        {
                            VulkanDeviceFeatureRequest{
                                .extensionName = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                            },
                        }},
            },
    },
    VulkanDeviceFeatureRequest{
        .extensionName = VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,
        .isRequired = false,
        .isSupportedFunc =
            [](const VulkanPhysicalDevice& physicalDevice) {
                return physicalDevice.queryFeatures<VkPhysicalDeviceFragmentDensityMapFeaturesEXT>().fragmentDensityMap ==
                       VK_TRUE;
            },
        .linkFunc =
            [](VulkanDeviceFeatureChain& featureChain) { featureChain.link(featureChain.fragmentDensityMapFeatures); },
    },
    VulkanDeviceFeatureRequest{
        .extensionName = VK_EXT_MESH_SHADER_EXTENSION_NAME,
        .isSupportedFunc =
            [](const VulkanPhysicalDevice& physicalDevice) {
                return physicalDevice.queryFeatures<VkPhysicalDeviceMeshShaderFeaturesEXT>().meshShader == VK_TRUE;
            },
        .linkFunc =
            [](VulkanDeviceFeatureChain& featureChain) {
                featureChain.link(featureChain.meshShaderFeatures);
                featureChain.link(featureChain.fragmentShadingRateFeatures);
            },
        .prerequisites =
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
        FlatStringHashSet extensionsToEnable;
        VulkanDeviceFeatureChain featureChainToEnable{};
        VulkanDeviceFeatures enabledFeatures{};
        for (const auto physicalDevice : physicalDevices) {
            const VulkanPhysicalDevice device(physicalDevice, instance.getApiVersion());

            VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            vkGetPhysicalDeviceProperties2(physicalDevice, &properties);
            spdlog::info("Checking {}", properties.properties.deviceName);
            featureChainToEnable.reset();
            extensionsToEnable.clear();
            enabledFeatures = {};
            if (!isPhysicalDeviceSuitable(
                    device, instance, kRequestedFeatures, featureChainToEnable, extensionsToEnable, enabledFeatures)) {
                spdlog::error("Device {} does not support all requested features.", properties.properties.deviceName);
            } else {
                selectedPhysicalDevice = physicalDevice;
                break;
            }
        }
        ASSERT_NE(selectedPhysicalDevice, VK_NULL_HANDLE);

        const std::vector<const char*> enabledExtensions(
            std::ranges::transform_view(extensionsToEnable, [](const auto& str) { return str.c_str(); }) |
            std::ranges::to<std::vector>());
        spdlog::info("Enabled extensions: {}", enabledExtensions.size());
        for (const auto& ext : enabledExtensions) {
            spdlog::info(" - {}", ext);
        }

        VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceCreateInfo.pNext = &featureChainToEnable.features;

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
