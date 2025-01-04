#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanResourceDeallocatorTest = VulkanTest;

TEST_F(VulkanResourceDeallocatorTest, DeferredDeallocation) {
    constexpr VkDeviceSize size = 100;

    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkBuffer buffer1 = device_->createBuffer(bufferInfo);
    const VkBuffer buffer2 = device_->createBuffer(bufferInfo);
    const VkBuffer buffer3 = device_->createBuffer(bufferInfo);

    const auto deferDeallocation = [&device = device_](int32_t framesToLive, VkBuffer buffer) {
        device->getResourceDeallocator().deferDestruction(framesToLive, buffer);
    };

    deferDeallocation(1, buffer1);
    deferDeallocation(2, buffer2);
    deferDeallocation(4, buffer3);
    device_->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device_->getResourceDeallocator().getDeferredDestructorCount(), 3);
    device_->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device_->getResourceDeallocator().getDeferredDestructorCount(), 2);
    device_->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device_->getResourceDeallocator().getDeferredDestructorCount(), 1);
    device_->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device_->getResourceDeallocator().getDeferredDestructorCount(), 1);
    device_->getResourceDeallocator().decrementLifetimes();
    EXPECT_EQ(device_->getResourceDeallocator().getDeferredDestructorCount(), 0);
}
} // namespace
} // namespace crisp