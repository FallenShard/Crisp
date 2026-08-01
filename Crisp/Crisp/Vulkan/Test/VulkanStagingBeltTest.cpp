#include <Crisp/Vulkan/VulkanStagingBelt.hpp>

#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanStagingBeltTest = VulkanTest;

constexpr VkDeviceSize kCapacity = 4096;

TEST_F(VulkanStagingBeltTest, PerFrameCycleDoesNotExhaustRing) {
    constexpr VkDeviceSize kSmallCapacity = 512;
    VulkanStagingBelt ctx(*device_, kSmallCapacity);

    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, 64, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        BufferMemoryType::GpuOnly);

    const std::vector<uint32_t> frameData(16, 42); // 64 bytes

    // Simulate several frames of uploads against a 512-byte ring. Each iteration reclaims the frame before it,
    // which is the only reason the ring never exhausts.
    for (uint64_t frame = 1; frame <= 20; ++frame) {
        ctx.collect(frame - 1);
        ctx.setRetirementValue(frame);

        ScopeCommandExecutor exec(*device_);
        ctx.uploadBuffer(exec.cmdBuffer.getHandle(), *dstBuffer, 0, frameData.data(), 64);
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
        BufferMemoryType::GpuOnly);

    const std::vector<uint32_t> data = {10, 20, 30, 40};

    ctx.setRetirementValue(1);
    {
        ScopeCommandExecutor exec(*device_);
        ctx.uploadBuffer(exec.cmdBuffer.getHandle(), *dstBuffer, 0, data.data(), 16);
    }
    ctx.collect(1);

    const auto readBack = toStdVec<uint32_t>(*dstBuffer);
    EXPECT_EQ(readBack, data);
}

// Ring space staged for work that has not completed must be held. The belt hands out a different chunk instead,
// which is the only externally visible sign that it refused to reuse the space.
TEST_F(VulkanStagingBeltTest, StagedRangeIsHeldUntilItsValueIsReached) {
    constexpr VkDeviceSize kRingCapacity = 2048;
    constexpr VkDeviceSize kAllocSize = 1024;
    const std::vector<uint8_t> data(kAllocSize, 0xEE);

    VulkanStagingBelt ctx(*device_, kRingCapacity);
    ctx.setRetirementValue(5);
    const auto first = ctx.stageData(data.data(), kAllocSize);
    const auto second = ctx.stageData(data.data(), kAllocSize);
    EXPECT_EQ(second.buffer, first.buffer);

    ctx.collect(4); // Below the stamp: nothing is reclaimable yet.
    const auto third = ctx.stageData(data.data(), kAllocSize);
    EXPECT_NE(third.buffer, first.buffer);
}

TEST_F(VulkanStagingBeltTest, StagedRangeIsReusedOnceItsValueIsReached) {
    constexpr VkDeviceSize kRingCapacity = 2048;
    constexpr VkDeviceSize kAllocSize = 1024;
    const std::vector<uint8_t> data(kAllocSize, 0xEE);

    VulkanStagingBelt ctx(*device_, kRingCapacity);
    ctx.setRetirementValue(5);
    const auto first = ctx.stageData(data.data(), kAllocSize);
    ctx.stageData(data.data(), kAllocSize);

    ctx.collect(5);
    const auto third = ctx.stageData(data.data(), kAllocSize);
    EXPECT_EQ(third.buffer, first.buffer);
    EXPECT_EQ(third.offset, first.offset);
}

// A staged range larger than the ring gets a chunk of its own rather than failing.
TEST_F(VulkanStagingBeltTest, OversizedStagingGetsItsOwnChunk) {
    constexpr VkDeviceSize kTinyCapacity = 64;
    const std::vector<uint8_t> bigData(256, 0xEE);

    VulkanStagingBelt ctx(*device_, kTinyCapacity);
    const auto tiny = ctx.stageData(bigData.data(), 32);
    const auto big = ctx.stageData(bigData.data(), bigData.size());

    EXPECT_NE(big.buffer, tiny.buffer);
    EXPECT_EQ(big.size, bigData.size());
}

} // namespace
} // namespace crisp
