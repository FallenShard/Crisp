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

    auto& deallocator = device_->getResourceDeallocator();

    const auto deferDeallocation = [&deallocator](int32_t framesToLive, VkBuffer buffer) {
        deallocator.deferDestruction(framesToLive, buffer);
    };

    deferDeallocation(1, buffer1);
    deferDeallocation(2, buffer2);
    deferDeallocation(4, buffer3);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 3);
    deallocator.advanceFrame();
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 3);
    deallocator.advanceFrame();
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 2);
    deallocator.advanceFrame();
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);
    deallocator.advanceFrame();
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);
    deallocator.advanceFrame();
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 0);
}
} // namespace
} // namespace crisp