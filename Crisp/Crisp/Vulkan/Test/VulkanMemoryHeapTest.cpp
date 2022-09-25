#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanMemoryAllocator.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <Crisp/Common/Result.hpp>
#include <glfw/glfw3.h>

using namespace crisp;

class VulkanMemoryHeapTest : public VulkanTest
{
};

using UniqueDeviceWrapper = UniqueHandleWrapper<
    VkDevice,
    [](VkDevice device)
    {
        vkDestroyDevice(device, nullptr);
    }>;

auto createMemoryHeapDeps()
{
    struct
    {
        std::unique_ptr<VulkanContext> context{
            std::make_unique<VulkanContext>(nullptr, std::vector<std::string>{}, false)};
        VulkanPhysicalDevice physicalDevice{context->selectPhysicalDevice({}).unwrap()};
        UniqueDeviceWrapper device{
            physicalDevice.createLogicalDevice(createDefaultQueueConfiguration(*context, physicalDevice))};
    } dependencies;

    return dependencies;
}

template <typename T>
std::unique_ptr<VulkanMemoryHeap> createHeap(
    const T& deps, const VkMemoryPropertyFlags memProps, const VkDeviceSize blockSize)
{
    const uint32_t heapIndex = deps.physicalDevice.findMemoryType(memProps).unwrap();
    return std::make_unique<VulkanMemoryHeap>(memProps, blockSize, heapIndex, deps.device, "");
}

TEST_F(VulkanMemoryHeapTest, StagingMemoryHeap)
{
    const auto deps = createMemoryHeapDeps();
    auto heap = createHeap(deps, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 1024);
    ASSERT_EQ(heap->getAllocatedSize(), 0);
    ASSERT_EQ(heap->getUsedSize(), 0);

    const auto allocation = heap->allocate(1024, 0).unwrap();
    ASSERT_EQ(heap->getUsedSize(), 1024);

    const auto allocation2 = heap->allocate(512, 0).unwrap();
    ASSERT_EQ(heap->getAllocatedSize(), 2048);
    ASSERT_EQ(heap->getUsedSize(), 1536);

    allocation.free();
    ASSERT_EQ(heap->getAllocatedSize(), 1024);
    ASSERT_EQ(heap->getUsedSize(), 512);

    allocation2.free();
    ASSERT_EQ(heap->getAllocatedSize(), 0);
    ASSERT_EQ(heap->getUsedSize(), 0);
}

TEST_F(VulkanMemoryHeapTest, DeviceMemoryHeap)
{
    const auto deps = createMemoryHeapDeps();
    auto heap = createHeap(deps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1 << 20);
    ASSERT_EQ(heap->getAllocatedSize(), 0);
    ASSERT_EQ(heap->getUsedSize(), 0);
}

TEST_F(VulkanMemoryHeapTest, Coalescing)
{
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
    ASSERT_EQ(block.freeChunks.size(), 1);
    ASSERT_EQ(block.freeChunks[0], 2040);

    block.freeChunks[0] = 1024;
    block.freeChunks[1024] = 512;
    block.freeChunks[1536] = 256;
    block.freeChunks[1792] = 128;

    block.freeChunks[1984] = 32;
    block.freeChunks[2016] = 16;
    block.freeChunks[2032] = 8;
    block.coalesce();
    ASSERT_EQ(block.freeChunks.size(), 2);
    ASSERT_EQ(block.freeChunks[0], 1920);
    ASSERT_EQ(block.freeChunks[1984], 56);
}
