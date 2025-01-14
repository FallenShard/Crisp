#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanDeviceTest = VulkanTest;

using ::testing::IsNull;
using ::testing::Not;

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

TEST_F(VulkanDeviceTest, CreateSemaphoreHandle) {
    const auto handle = device_->createSemaphore();
    EXPECT_THAT(handle, Not(IsNull()));
    vkDestroySemaphore(device_->getHandle(), handle, nullptr);
}

TEST_F(VulkanDeviceTest, createFenceHandle) {
    {
        const auto handle = device_->createFence();
        EXPECT_THAT(handle, Not(IsNull()));
        vkDestroyFence(device_->getHandle(), handle, nullptr);
    }
    {
        const auto handle = device_->createFence(VK_FENCE_CREATE_SIGNALED_BIT);
        EXPECT_THAT(handle, Not(IsNull()));
        vkDestroyFence(device_->getHandle(), handle, nullptr);
    }
}

} // namespace
} // namespace crisp