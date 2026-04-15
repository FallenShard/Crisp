#include <Crisp/Vulkan/VulkanStagingBelt.hpp>

#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanStagingBeltTest = VulkanTest;

constexpr VkDeviceSize kCapacity = 4096;

TEST_F(VulkanStagingBeltTest, UploadBufferBlocking) {
    VulkanStagingBelt ctx(*device_, kCapacity);

    const std::vector<uint32_t> srcData = {1, 2, 3, 4, 5, 6, 7, 8};
    const VkDeviceSize dataSize = srcData.size() * sizeof(uint32_t);

    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, dataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ctx.uploadBufferBlocking(device_->getGeneralQueue(), *dstBuffer, srcData.data(), dataSize);

    const auto readBack = toStdVec<uint32_t>(*dstBuffer);
    EXPECT_EQ(readBack, srcData);
}

TEST_F(VulkanStagingBeltTest, UploadBufferBlockingWithOffset) {
    VulkanStagingBelt ctx(*device_, kCapacity);

    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, 32, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Zero-fill the buffer first.
    const std::vector<uint32_t> zeros(8, 0);
    ctx.uploadBufferBlocking(device_->getGeneralQueue(), *dstBuffer, zeros.data(), 32);

    // Write 2 uint32s at byte offset 8.
    const std::vector<uint32_t> patch = {0xAA, 0xBB};
    ctx.uploadBufferBlocking(device_->getGeneralQueue(), *dstBuffer, 8, patch.data(), 8);

    const auto readBack = toStdVec<uint32_t>(*dstBuffer);
    EXPECT_EQ(readBack[0], 0u);
    EXPECT_EQ(readBack[1], 0u);
    EXPECT_EQ(readBack[2], 0xAAu);
    EXPECT_EQ(readBack[3], 0xBBu);
    EXPECT_EQ(readBack[4], 0u);
}

TEST_F(VulkanStagingBeltTest, PerFrameCycleDoesNotExhaustRing) {
    constexpr VkDeviceSize kSmallCapacity = 512;
    VulkanStagingBelt ctx(*device_, kSmallCapacity);

    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, 64, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    const std::vector<uint32_t> frameData(16, 42); // 64 bytes

    // Simulate several frames of double-buffered uploads.
    // With a 512-byte ring and 64-byte uploads per frame, the ring should reclaim
    // correctly and never exhaust.
    for (int frame = 0; frame < 20; ++frame) {
        const uint32_t vfi = frame % 2;

        ctx.beginFrame(vfi);

        ScopeCommandExecutor exec(*device_);
        ctx.uploadBuffer(exec.cmdBuffer.getHandle(), *dstBuffer, 0, frameData.data(), 64);

        ctx.endFrame();
    }

    // If we got here without hitting CRISP_CHECK, the ring reclamation works.
    const auto readBack = toStdVec<uint32_t>(*dstBuffer);
    for (const auto& val : readBack) {
        EXPECT_EQ(val, 42u);
    }
}

TEST_F(VulkanStagingBeltTest, UploadViaPerFramePathVerifyContents) {
    VulkanStagingBelt ctx(*device_, kCapacity);

    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    const std::vector<uint32_t> data = {10, 20, 30, 40};

    ctx.beginFrame(0);
    {
        ScopeCommandExecutor exec(*device_);
        ctx.uploadBuffer(exec.cmdBuffer.getHandle(), *dstBuffer, 0, data.data(), 16);
    }
    ctx.endFrame();

    const auto readBack = toStdVec<uint32_t>(*dstBuffer);
    EXPECT_EQ(readBack, data);
}

TEST_F(VulkanStagingBeltTest, OversizedBlockingUploadUsesTemporaryBuffer) {
    constexpr VkDeviceSize kTinyCapacity = 64;
    VulkanStagingBelt ctx(*device_, kTinyCapacity);

    // Upload 256 bytes, which exceeds the 64-byte ring capacity.
    const std::vector<uint32_t> bigData(64, 0xDEAD); // 256 bytes
    const VkDeviceSize dataSize = bigData.size() * sizeof(uint32_t);

    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, dataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ctx.uploadBufferBlocking(device_->getGeneralQueue(), *dstBuffer, bigData.data(), dataSize);

    const auto readBack = toStdVec<uint32_t>(*dstBuffer);
    EXPECT_EQ(readBack, bigData);
}

} // namespace
} // namespace crisp
