#include <Crisp/Vulkan/Test/VulkanTest.hpp>

namespace crisp::test {
namespace {
using VulkanDeviceTest = VulkanTest;

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::SizeIs;

TEST_F(VulkanDeviceTest, ValidHandle) {
    EXPECT_THAT(device_->getHandle(), Not(IsNull()));
}

TEST_F(VulkanDeviceTest, Queues) {
    EXPECT_TRUE(device_->getGeneralQueue().supportsOperations(VK_QUEUE_GRAPHICS_BIT));
    EXPECT_TRUE(device_->getGeneralQueue().supportsOperations(VK_QUEUE_TRANSFER_BIT));
    EXPECT_TRUE(device_->getGeneralQueue().supportsOperations(VK_QUEUE_COMPUTE_BIT));
    EXPECT_TRUE(device_->getComputeQueue().supportsOperations(VK_QUEUE_TRANSFER_BIT));
    EXPECT_TRUE(device_->getComputeQueue().supportsOperations(VK_QUEUE_COMPUTE_BIT));
    EXPECT_TRUE(device_->getTransferQueue().supportsOperations(VK_QUEUE_TRANSFER_BIT));
}

TEST_F(VulkanDeviceTest, NonCoherentMappedRanges) {
    const VkDeviceSize atomSize = physicalDevice_->getLimits().nonCoherentAtomSize;

    constexpr uint32_t chunks = 10;
    constexpr VkDeviceSize chunkSize = 100;
    for (uint32_t i = 0; i < chunks; ++i) {
        const auto chunk = device_->getMemoryAllocator().getDeviceBufferHeap().allocate(chunkSize, atomSize).unwrap();
        EXPECT_EQ(chunk.offset % atomSize, 0);
        EXPECT_EQ(chunk.size, chunkSize);
    }

    EXPECT_EQ(device_->getMemoryAllocator().getDeviceMemoryUsage().bufferMemoryUsed, chunks * chunkSize);
}
} // namespace
} // namespace crisp::test