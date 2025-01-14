#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanResourceDeallocatorTest = VulkanTest;

VkBuffer createBuffer(const VulkanDevice& device, const VkBufferCreateInfo& createInfo) {
    VkBuffer buffer{};
    vkCreateBuffer(device.getHandle(), &createInfo, nullptr, &buffer);
    return buffer;
}

TEST_F(VulkanResourceDeallocatorTest, DeferredDeallocation) {
    constexpr VkDeviceSize size = 100;

    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkBuffer buffer1 = createBuffer(*device_, bufferInfo);
    const VkBuffer buffer2 = createBuffer(*device_, bufferInfo);
    const VkBuffer buffer3 = createBuffer(*device_, bufferInfo);

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