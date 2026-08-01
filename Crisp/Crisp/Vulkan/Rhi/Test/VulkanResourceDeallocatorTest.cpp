#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanResourceDeallocatorTest = VulkanTest;

VkBuffer createBuffer(const VulkanDevice& device, const VkBufferCreateInfo& createInfo) {
    VkBuffer buffer{};
    vkCreateBuffer(device.getHandle(), &createInfo, nullptr, &buffer);
    return buffer;
}

VkBuffer createTransferBuffer(const VulkanDevice& device) {
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = 100;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    return createBuffer(device, bufferInfo);
}

TEST_F(VulkanResourceDeallocatorTest, DeferredDeallocation) {
    auto& deallocator = device_->getResourceDeallocator();

    // Two frames' worth of destruction, stamped with the value each frame's submission signals.
    deallocator.setRetirementValue(1);
    deallocator.deferDestruction(createTransferBuffer(*device_));
    deallocator.deferDestruction(createTransferBuffer(*device_));
    deallocator.setRetirementValue(2);
    deallocator.deferDestruction(createTransferBuffer(*device_));

    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 3);
    deallocator.collect(0);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 3);
    deallocator.collect(1);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);
    deallocator.collect(2);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 0);
    deallocator.collect(3);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 0);
}

TEST_F(VulkanResourceDeallocatorTest, DestructionRetiresAtTheStampedValue) {
    constexpr uint64_t retirementValue = 7;

    auto& deallocator = device_->getResourceDeallocator();
    deallocator.setRetirementValue(retirementValue);
    deallocator.deferDestruction(createTransferBuffer(*device_));
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);

    deallocator.collect(retirementValue - 1);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);

    deallocator.collect(retirementValue);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 0);
}

TEST_F(VulkanResourceDeallocatorTest, DestructionRetiresHandleAndMemoryTogether) {
    constexpr uint64_t retirementValue = 11;

    auto& deallocator = device_->getResourceDeallocator();
    deallocator.setRetirementValue(retirementValue);
    {
        const VulkanBuffer buffer(*device_, 256, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferMemoryType::HostUpload);
    }

    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);
    EXPECT_EQ(deallocator.getDeferredMemoryDeallocationCount(), 1);

    deallocator.collect(retirementValue - 1);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);
    EXPECT_EQ(deallocator.getDeferredMemoryDeallocationCount(), 1);

    deallocator.collect(retirementValue);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 0);
    EXPECT_EQ(deallocator.getDeferredMemoryDeallocationCount(), 0);
}

// The frame-throttle path relies on this: a resource destroyed while a frame is recorded must outlive that frame's
// submission, not be retired by the value the previous one already signaled.
TEST_F(VulkanResourceDeallocatorTest, LaterStampSurvivesAnEarlierCollect) {
    auto& deallocator = device_->getResourceDeallocator();

    deallocator.setRetirementValue(4);
    deallocator.deferDestruction(createTransferBuffer(*device_));
    deallocator.setRetirementValue(5);
    deallocator.deferDestruction(createTransferBuffer(*device_));

    deallocator.collect(4);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);

    deallocator.collect(5);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 0);
}

TEST_F(VulkanResourceDeallocatorTest, ExplicitStampIgnoresTheCurrentRetirementValue) {
    auto& deallocator = device_->getResourceDeallocator();

    deallocator.setRetirementValue(2);
    deallocator.deferDestructionAt(9, createTransferBuffer(*device_));

    deallocator.collect(2);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 1);

    deallocator.collect(9);
    EXPECT_EQ(deallocator.getDeferredDestructorCount(), 0);
}
} // namespace
} // namespace crisp
