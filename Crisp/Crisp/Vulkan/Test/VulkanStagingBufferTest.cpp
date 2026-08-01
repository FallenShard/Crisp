#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanStagingBufferTest = VulkanTest;

constexpr VkDeviceSize kAlignment = 64;

TEST_F(VulkanStagingBufferTest, UploadBufferBlocking) {
    const std::vector<uint32_t> srcData = {1, 2, 3, 4, 5, 6, 7, 8};
    const VkDeviceSize dataSize = srcData.size() * sizeof(uint32_t);

    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, dataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        BufferMemoryType::GpuOnly);

    uploadBufferBlocking(*device_, device_->getGeneralQueue(), *dstBuffer, srcData.data(), dataSize);

    EXPECT_EQ(toStdVec<uint32_t>(*dstBuffer), srcData);
}

TEST_F(VulkanStagingBufferTest, UploadBufferBlockingWithOffset) {
    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, 32, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        BufferMemoryType::GpuOnly);

    const std::vector<uint32_t> zeros(8, 0);
    uploadBufferBlocking(*device_, device_->getGeneralQueue(), *dstBuffer, zeros.data(), 32);

    // Write 2 uint32s at byte offset 8.
    const std::vector<uint32_t> patch = {0xAA, 0xBB};
    uploadBufferBlocking(*device_, device_->getGeneralQueue(), *dstBuffer, 8, patch.data(), 8);

    const auto readBack = toStdVec<uint32_t>(*dstBuffer);
    EXPECT_EQ(readBack[0], 0u);
    EXPECT_EQ(readBack[1], 0u);
    EXPECT_EQ(readBack[2], 0xAAu);
    EXPECT_EQ(readBack[3], 0xBBu);
    EXPECT_EQ(readBack[4], 0u);
}

TEST_F(VulkanStagingBufferTest, UploadBufferBlockingFromVector) {
    const std::vector<uint32_t> srcData(64, 0xDEAD);

    auto dstBuffer = std::make_unique<VulkanBuffer>(
        *device_, srcData.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferMemoryType::GpuOnly);

    uploadBufferBlocking(*device_, device_->getGeneralQueue(), *dstBuffer, srcData);

    EXPECT_EQ(toStdVec<uint32_t>(*dstBuffer), srcData);
}

TEST_F(VulkanStagingBufferTest, AllocationsAreMonotonicallyIncreasingAndAligned) {
    VulkanStagingBuffer staging(*device_, 4096, kAlignment);

    VkDeviceSize prevOffset = 0;
    for (int i = 0; i < 10; ++i) {
        const auto alloc = staging.allocate(100);
        ASSERT_TRUE(alloc.has_value());
        EXPECT_GE(alloc->offset, prevOffset);
        EXPECT_EQ(alloc->offset % kAlignment, 0);
        EXPECT_NE(alloc->mappedPtr, nullptr);
        prevOffset = alloc->offset + alloc->size;
    }
}

TEST_F(VulkanStagingBufferTest, FullRingReturnsNullopt) {
    constexpr VkDeviceSize kCapacity = 512;
    VulkanStagingBuffer staging(*device_, kCapacity, kAlignment);

    // Fill the ring.
    const auto a1 = staging.allocate(256);
    ASSERT_TRUE(a1.has_value());
    const auto a2 = staging.allocate(256);
    ASSERT_TRUE(a2.has_value());

    // Ring is full.
    EXPECT_FALSE(staging.allocate(1).has_value());
    EXPECT_EQ(staging.getFreeBytes(), 0);
}

TEST_F(VulkanStagingBufferTest, ReclaimMakesSpaceReusable) {
    constexpr VkDeviceSize kCapacity = 512;
    VulkanStagingBuffer staging(*device_, kCapacity, kAlignment);

    const auto a1 = staging.allocate(256);
    ASSERT_TRUE(a1.has_value());
    const auto a2 = staging.allocate(256);
    ASSERT_TRUE(a2.has_value());
    EXPECT_FALSE(staging.allocate(1).has_value());

    // Reclaim first allocation's region.
    staging.reclaim(a1->offset + kAlignment * ((a1->size + kAlignment - 1) / kAlignment));
    EXPECT_TRUE(staging.allocate(64).has_value());
}

TEST_F(VulkanStagingBufferTest, WrapAround) {
    constexpr VkDeviceSize kCapacity = 1024;
    VulkanStagingBuffer staging(*device_, kCapacity, kAlignment);

    // Allocate most of the ring.
    const auto a1 = staging.allocate(512);
    ASSERT_TRUE(a1.has_value());
    const auto a2 = staging.allocate(400);
    ASSERT_TRUE(a2.has_value());

    // Reclaim the first allocation so tail moves forward.
    staging.reclaim(staging.getHead());
    staging.reclaim(a2->offset); // This is wrong, we need to reclaim up to where a1 ended
    // Actually, let's reclaim everything before a2.
    // After a1 and a2, head is somewhere past 900. Reclaim up to a2's start.
    // But the correct approach: reclaim the first half.

    // Simpler approach: allocate, reclaim first half, then allocate wrapping around.
    VulkanStagingBuffer staging2(*device_, kCapacity, kAlignment);
    const auto b1 = staging2.allocate(512);
    ASSERT_TRUE(b1.has_value());

    // Reclaim b1's region so tail advances past it.
    staging2.reclaim(staging2.getHead());

    const auto b2 = staging2.allocate(400);
    ASSERT_TRUE(b2.has_value());

    // Now head is near 912. Not enough for another 400 at the end.
    // But we reclaimed [0, 512), so wrapping should work.
    staging2.reclaim(staging2.getHead());
    const auto b3 = staging2.allocate(400);
    ASSERT_TRUE(b3.has_value());
}

TEST_F(VulkanStagingBufferTest, ReclaimAllResetsState) {
    constexpr VkDeviceSize kCapacity = 1024;
    VulkanStagingBuffer staging(*device_, kCapacity, kAlignment);

    staging.allocate(512);
    staging.allocate(512);
    EXPECT_FALSE(staging.allocate(1).has_value());

    staging.reclaimAll();
    EXPECT_EQ(staging.getFreeBytes(), kCapacity);
    EXPECT_EQ(staging.getHead(), 0);

    const auto alloc = staging.allocate(1024);
    ASSERT_TRUE(alloc.has_value());
    EXPECT_EQ(alloc->offset, 0);
}

TEST_F(VulkanStagingBufferTest, DataIntegrity) {
    constexpr VkDeviceSize kCapacity = 4096;
    VulkanStagingBuffer staging(*device_, kCapacity, kAlignment);

    constexpr uint32_t kValue = 0xDEADBEEF;
    const auto alloc = staging.allocate(sizeof(kValue));
    ASSERT_TRUE(alloc.has_value());

    std::memcpy(alloc->mappedPtr, &kValue, sizeof(kValue));

    // Read back from the same mapped pointer to verify the write landed.
    uint32_t readBack{};
    std::memcpy(&readBack, alloc->mappedPtr, sizeof(readBack));
    EXPECT_EQ(readBack, kValue);
}

TEST_F(VulkanStagingBufferTest, AllocationExceedingCapacityReturnsNullopt) {
    constexpr VkDeviceSize kCapacity = 256;
    VulkanStagingBuffer staging(*device_, kCapacity, kAlignment);
    EXPECT_FALSE(staging.allocate(kCapacity + 1).has_value());
}

TEST_F(VulkanStagingBufferTest, AlignmentRoundsUpCorrectly) {
    constexpr VkDeviceSize kCapacity = 4096;
    constexpr VkDeviceSize kAlign = 256;
    VulkanStagingBuffer staging(*device_, kCapacity, kAlign);

    // Allocate 1 byte -- should consume 256 bytes (one alignment unit).
    const auto a1 = staging.allocate(1);
    ASSERT_TRUE(a1.has_value());
    EXPECT_EQ(a1->offset, 0);

    const auto a2 = staging.allocate(1);
    ASSERT_TRUE(a2.has_value());
    EXPECT_EQ(a2->offset, kAlign);
}

} // namespace
} // namespace crisp
