#include <VulkanTests/VulkanTest.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <CrispCore/Result.hpp>

using namespace crisp;

class VulkanDeviceTest : public VulkanTest
{
};

TEST_F(VulkanDeviceTest, ValidHandle)
{
    const auto& [deps, device] = createDevice();
    ASSERT_NE(device->getHandle(), nullptr);
    ASSERT_EQ(device->getPhysicalDevice().getHandle(), deps.physicalDevice->getHandle());
}

TEST_F(VulkanDeviceTest, Queues)
{
    const auto& [deps, device] = createDevice();
    ASSERT_TRUE(device->getGeneralQueue().supportsOperations(VK_QUEUE_GRAPHICS_BIT));
    ASSERT_TRUE(device->getGeneralQueue().supportsOperations(VK_QUEUE_TRANSFER_BIT));
    ASSERT_TRUE(device->getGeneralQueue().supportsOperations(VK_QUEUE_COMPUTE_BIT));
    ASSERT_TRUE(device->getComputeQueue().supportsOperations(VK_QUEUE_TRANSFER_BIT));
    ASSERT_TRUE(device->getComputeQueue().supportsOperations(VK_QUEUE_COMPUTE_BIT));
    ASSERT_TRUE(device->getTransferQueue().supportsOperations(VK_QUEUE_TRANSFER_BIT));
}

TEST_F(VulkanDeviceTest, NonCoherentMappedRanges)
{
    const auto& [deps, device] = createDevice();
    const VkDeviceSize atomSize = deps.physicalDevice->getLimits().nonCoherentAtomSize;

    for (uint32_t i = 0; i < 10; ++i)
    {
        const auto chunk = device->getMemoryAllocator().getDeviceBufferHeap().allocate(100, atomSize).unwrap();
        EXPECT_EQ(chunk.offset % atomSize, 0);
        EXPECT_EQ(chunk.size, 100);
    }

    EXPECT_EQ(device->getMemoryAllocator().getDeviceMemoryUsage().bufferMemoryUsed, 100 * 10);
}