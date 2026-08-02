#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

#include <Crisp/Renderer/BindlessImageRegistry.hpp>

#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>

namespace crisp {
namespace {

using ::testing::Ne;

using BindlessImageRegistryTest = VulkanTest;

constexpr BindlessImageRegistryConfig kSmallConfig{
    .sampledImageCapacity = 8,
    .storageImageCapacity = 4,
    .samplerCapacity = 2,
};

std::unique_ptr<VulkanImage> createSampledImage(const VulkanDevice& device) {
    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.arrayLayers = 1;
    createInfo.extent = {4, 4, 1};
    createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    createInfo.mipLevels = 1;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    return std::make_unique<VulkanImage>(device, createInfo);
}

std::unique_ptr<VulkanImage> createStorageImage(const VulkanDevice& device) {
    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.arrayLayers = 1;
    createInfo.extent = {4, 4, 1};
    createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    createInfo.mipLevels = 1;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    return std::make_unique<VulkanImage>(device, createInfo);
}

TEST_F(BindlessImageRegistryTest, CreatesSetAndLayout) {
    const BindlessImageRegistry registry(*device_, *physicalDevice_, kSmallConfig);

    EXPECT_THAT(registry.getSetLayout(), Ne(VK_NULL_HANDLE));
    EXPECT_THAT(registry.getSet(), Ne(VK_NULL_HANDLE));
    EXPECT_EQ(registry.getSampledImageCapacity(), kSmallConfig.sampledImageCapacity);
    EXPECT_EQ(registry.getStorageImageCapacity(), kSmallConfig.storageImageCapacity);
}

TEST_F(BindlessImageRegistryTest, DefaultFillCoversEverySlot) {
    BindlessImageRegistry registry(*device_, *physicalDevice_, kSmallConfig);
    const auto image = createSampledImage(*device_);
    const auto view = createView(*device_, *image, VK_IMAGE_VIEW_TYPE_2D);

    registry.setDefaultSampledImage(*view);

    // Every slot, not just slot 0 - an unwritten PARTIALLY_BOUND descriptor is undefined if read.
    EXPECT_EQ(registry.getPendingWriteCount(), kSmallConfig.sampledImageCapacity);
    registry.flush();
    EXPECT_EQ(registry.getPendingWriteCount(), 0);
}

TEST_F(BindlessImageRegistryTest, HandsOutAscendingIndicesSkippingTheDefaultSlot) {
    BindlessImageRegistry registry(*device_, *physicalDevice_, kSmallConfig);
    const auto image = createSampledImage(*device_);
    const auto view = createView(*device_, *image, VK_IMAGE_VIEW_TYPE_2D);
    registry.setDefaultSampledImage(*view);

    const auto first = registry.addSampledImage(*view, "test-sampled");
    const auto second = registry.addSampledImage(*view, "test-sampled");

    EXPECT_EQ(first.index(), BindlessImageRegistry::kDefaultSlot + 1);
    EXPECT_EQ(second.index(), BindlessImageRegistry::kDefaultSlot + 2);
    EXPECT_TRUE(first.isValid());
    registry.flush();
}

TEST_F(BindlessImageRegistryTest, SampledAndStorageArraysHaveIndependentIndexSpaces) {
    BindlessImageRegistry registry(*device_, *physicalDevice_, kSmallConfig);
    const auto sampled = createSampledImage(*device_);
    const auto sampledView = createView(*device_, *sampled, VK_IMAGE_VIEW_TYPE_2D);
    const auto storage = createStorageImage(*device_);
    const auto storageView = createView(*device_, *storage, VK_IMAGE_VIEW_TYPE_2D);
    registry.setDefaultSampledImage(*sampledView);
    registry.setDefaultStorageImage(*storageView);

    const auto sampledHandle = registry.addSampledImage(*sampledView, "test-sampled");
    const auto storageHandle = registry.addStorageImage(*storageView, "test-storage");

    EXPECT_EQ(sampledHandle.index(), storageHandle.index());
    registry.flush();
}

TEST_F(BindlessImageRegistryTest, ReleasedSlotIsNotReusableUntilTheGpuHasPassedIt) {
    BindlessImageRegistry registry(*device_, *physicalDevice_, kSmallConfig);
    const auto image = createSampledImage(*device_);
    const auto view = createView(*device_, *image, VK_IMAGE_VIEW_TYPE_2D);
    registry.setDefaultSampledImage(*view);
    registry.flush();

    registry.setRetirementValue(10);
    const auto handle = registry.addSampledImage(*view, "test-sampled");
    registry.remove(handle);

    registry.collect(9);
    const auto beforeRetirement = registry.addSampledImage(*view, "test-sampled");
    EXPECT_THAT(beforeRetirement.index(), Ne(handle.index()));

    registry.collect(10);
    const auto afterRetirement = registry.addSampledImage(*view, "test-sampled");
    EXPECT_EQ(afterRetirement.index(), handle.index());

    // Same slot, but the recycled handle carries a generation that no longer matches.
    EXPECT_THAT(afterRetirement.generation(), Ne(handle.generation()));
    registry.flush();
}

TEST_F(BindlessImageRegistryTest, CollectRestoresTheDefaultIntoFreedSlots) {
    BindlessImageRegistry registry(*device_, *physicalDevice_, kSmallConfig);
    const auto image = createSampledImage(*device_);
    const auto view = createView(*device_, *image, VK_IMAGE_VIEW_TYPE_2D);
    registry.setDefaultSampledImage(*view);
    registry.flush();

    registry.setRetirementValue(1);
    const auto handle = registry.addSampledImage(*view, "test-sampled");
    registry.flush();

    registry.remove(handle);
    EXPECT_EQ(registry.getPendingWriteCount(), 0);

    registry.collect(1);
    EXPECT_EQ(registry.getPendingWriteCount(), 1);
    registry.flush();
}

TEST_F(BindlessImageRegistryTest, SlotDebugInfoTracksTheLifecycle) {
    BindlessImageRegistry registry(*device_, *physicalDevice_, kSmallConfig);
    const auto image = createSampledImage(*device_);
    const auto view = createView(*device_, *image, VK_IMAGE_VIEW_TYPE_2D);
    registry.setDefaultSampledImage(*view);

    registry.setRetirementValue(7);
    const auto handle = registry.addSampledImage(*view, "sponza/lion_albedo");

    const auto slots = registry.getSampledImageSlots();
    EXPECT_EQ(slots.size(), kSmallConfig.sampledImageCapacity);
    EXPECT_EQ(slots[handle.index()].name, "sponza/lion_albedo");
    EXPECT_EQ(slots[handle.index()].registeredAt, 7);
    EXPECT_EQ(slots[handle.index()].state, BindlessSlotState::Live);
    EXPECT_EQ(slots[BindlessImageRegistry::kDefaultSlot].state, BindlessSlotState::Live);

    registry.setRetirementValue(8);
    registry.remove(handle);
    EXPECT_EQ(slots[handle.index()].state, BindlessSlotState::Retiring);
    EXPECT_EQ(slots[handle.index()].releasedAt, 8);

    registry.collect(8);
    EXPECT_EQ(slots[handle.index()].state, BindlessSlotState::Free);

    // The name survives the free: a stale index is far easier to diagnose when the slot can say what it held.
    EXPECT_EQ(slots[handle.index()].name, "sponza/lion_albedo");
    registry.flush();
}

TEST_F(BindlessImageRegistryTest, SamplersAreAppendOnly) {
    BindlessImageRegistry registry(*device_, *physicalDevice_, kSmallConfig);
    const auto linear = createLinearClampSampler(*device_);
    const auto nearest = createNearestClampSampler(*device_);

    EXPECT_EQ(registry.addSampler(*linear, "linearClamp"), 0);
    EXPECT_EQ(registry.addSampler(*nearest, "nearestClamp"), 1);
    EXPECT_EQ(registry.getSamplerCount(), 2);

    const auto slots = registry.getSamplerSlots();
    ASSERT_EQ(slots.size(), 2);
    EXPECT_EQ(slots[0].name, "linearClamp");
    EXPECT_EQ(slots[1].name, "nearestClamp");
    registry.flush();
}

TEST_F(BindlessImageRegistryTest, CapacityIsClampedToDeviceLimits) {
    const BindlessImageRegistryConfig hugeConfig{
        .sampledImageCapacity = ~0U,
        .storageImageCapacity = ~0U,
        .samplerCapacity = ~0U,
    };
    const BindlessImageRegistry registry(*device_, *physicalDevice_, hugeConfig);

    const auto& limits = physicalDevice_->getCapabilities().properties12;
    EXPECT_LE(registry.getSampledImageCapacity(), limits.maxDescriptorSetUpdateAfterBindSampledImages);
    EXPECT_LE(registry.getStorageImageCapacity(), limits.maxDescriptorSetUpdateAfterBindStorageImages);
}

} // namespace
} // namespace crisp
