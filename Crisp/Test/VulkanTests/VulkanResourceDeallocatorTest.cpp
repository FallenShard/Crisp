#include <Test/VulkanTests/ScopeCommandBufferExecutor.hpp>
#include <Test/VulkanTests/VulkanTest.hpp>

#include <numeric>

using namespace crisp;

class VulkanResourceDeallocatorTest : public VulkanTest
{
};

TEST_F(VulkanResourceDeallocatorTest, DeferredDeallocation)
{
    const auto& [deps, device] = createDevice();
    constexpr VkDeviceSize size = 100;

    // Create a buffer handle
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkBuffer buffer1 = device->createBuffer(bufferInfo);
    const VkBuffer buffer2 = device->createBuffer(bufferInfo);
    const VkBuffer buffer3 = device->createBuffer(bufferInfo);

    const auto deferDeallocation = [&device](int32_t framesToLive, VkBuffer buffer)
    {
        device->getResourceDeallocator().deferDestruction(framesToLive, buffer);
    };

    deferDeallocation(1, buffer1);
    deferDeallocation(2, buffer2);
    deferDeallocation(4, buffer3);
    device->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device->getResourceDeallocator().getDeferredDestructorCount(), 3);
    device->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device->getResourceDeallocator().getDeferredDestructorCount(), 2);
    device->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device->getResourceDeallocator().getDeferredDestructorCount(), 1);
    device->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device->getResourceDeallocator().getDeferredDestructorCount(), 1);
    device->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device->getResourceDeallocator().getDeferredDestructorCount(), 0);
}
