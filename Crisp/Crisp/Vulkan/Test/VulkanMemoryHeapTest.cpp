#include <Crisp/Vulkan/Test/VulkanTest.hpp>

namespace crisp::test {
namespace {
using ::testing::SizeIs;

class VulkanMemoryHeapTest : public VulkanTest {
protected:
    std::unique_ptr<VulkanMemoryHeap> createHeap(const VkMemoryPropertyFlags memProps, const VkDeviceSize blockSize) {
        const uint32_t heapIndex = physicalDevice_->findMemoryType(memProps).unwrap();
        return std::make_unique<VulkanMemoryHeap>(memProps, blockSize, heapIndex, device_->getHandle(), "");
    }
};

TEST_F(VulkanMemoryHeapTest, StagingMemoryHeap) {
    auto heap = createHeap(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 1024);
    EXPECT_EQ(heap->getAllocatedSize(), 0);
    EXPECT_EQ(heap->getUsedSize(), 0);

    const auto allocation = heap->allocate(1024, 0).unwrap();
    EXPECT_EQ(heap->getUsedSize(), 1024);

    const auto allocation2 = heap->allocate(512, 0).unwrap();
    EXPECT_EQ(heap->getAllocatedSize(), 2048);
    EXPECT_EQ(heap->getUsedSize(), 1536);

    allocation.free();
    EXPECT_EQ(heap->getAllocatedSize(), 1024);
    EXPECT_EQ(heap->getUsedSize(), 512);

    allocation2.free();
    EXPECT_EQ(heap->getAllocatedSize(), 0);
    EXPECT_EQ(heap->getUsedSize(), 0);

    EXPECT_THAT(heap->allocate(1025, 0), HasError());
}

TEST_F(VulkanMemoryHeapTest, DeviceMemoryHeap) {
    auto heap = createHeap(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1 << 20);
    EXPECT_EQ(heap->getAllocatedSize(), 0);
    EXPECT_EQ(heap->getUsedSize(), 0);
}

TEST_F(VulkanMemoryHeapTest, Coalescing) {
    VulkanMemoryHeap::AllocationBlock block{};
    block.freeChunks[0] = 1024;
    block.freeChunks[1024] = 512;
    block.freeChunks[1536] = 256;
    block.freeChunks[1792] = 128;
    block.freeChunks[1920] = 64;
    block.freeChunks[1984] = 32;
    block.freeChunks[2016] = 16;
    block.freeChunks[2032] = 8;
    block.coalesce();
    ASSERT_THAT(block.freeChunks, SizeIs(1));
    EXPECT_EQ(block.freeChunks[0], 2040);

    block.freeChunks[0] = 1024;
    block.freeChunks[1024] = 512;
    block.freeChunks[1536] = 256;
    block.freeChunks[1792] = 128;

    block.freeChunks[1984] = 32;
    block.freeChunks[2016] = 16;
    block.freeChunks[2032] = 8;
    block.coalesce();
    ASSERT_THAT(block.freeChunks, SizeIs(2));
    EXPECT_EQ(block.freeChunks[0], 1920);
    ASSERT_EQ(block.freeChunks[1984], 56);
}
} // namespace
} // namespace crisp::test