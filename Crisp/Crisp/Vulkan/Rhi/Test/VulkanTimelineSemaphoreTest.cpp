#include <Crisp/Vulkan/Rhi/VulkanTimelineSemaphore.hpp>

#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanTimelineSemaphoreTest = VulkanTest;

using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::Not;

// Submits an empty command buffer that signals the timeline's next value, and returns that value.
uint64_t submitEmptyWorkload(
    const VulkanDevice& device, const VulkanCommandPool& commandPool, VulkanTimelineSemaphore& timeline) {
    VulkanCommandBuffer cmdBuffer(commandPool.allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    cmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    cmdBuffer.end();

    const uint64_t target = timeline.advance();
    const VkCommandBufferSubmitInfo cmdBufferInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdBuffer.getHandle(),
    };
    const VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = timeline.getHandle(),
        .value = target,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    const VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufferInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };
    EXPECT_THAT(vkQueueSubmit2(device.getGeneralQueue().getHandle(), 1, &submitInfo, VK_NULL_HANDLE), Eq(VK_SUCCESS));
    return target;
}

TEST_F(VulkanTimelineSemaphoreTest, CreatedWithInitialValue) {
    const VulkanTimelineSemaphore timeline(*device_, 7);
    EXPECT_THAT(timeline.getHandle(), Not(IsNull()));
    EXPECT_THAT(timeline.getCompletedValue(), Eq(7));
    EXPECT_THAT(timeline.getScheduledValue(), Eq(7));
}

TEST_F(VulkanTimelineSemaphoreTest, DefaultsToZero) {
    const VulkanTimelineSemaphore timeline(*device_);
    EXPECT_THAT(timeline.getCompletedValue(), Eq(0));
    EXPECT_THAT(timeline.getScheduledValue(), Eq(0));
    // The value a never-submitted frame slot carries, so waiting on it must be a no-op rather than a deadlock.
    EXPECT_THAT(timeline.isComplete(0), IsTrue());
    timeline.wait(0);
}

TEST_F(VulkanTimelineSemaphoreTest, AdvanceReservesWithoutSignaling) {
    VulkanTimelineSemaphore timeline(*device_);

    EXPECT_THAT(timeline.advance(), Eq(1));
    EXPECT_THAT(timeline.advance(), Eq(2));
    EXPECT_THAT(timeline.getScheduledValue(), Eq(2));

    // advance() is CPU-side bookkeeping - nothing has signaled the counter yet.
    EXPECT_THAT(timeline.getCompletedValue(), Eq(0));
    EXPECT_THAT(timeline.isComplete(1), IsFalse());
}

TEST_F(VulkanTimelineSemaphoreTest, HostSignal) {
    VulkanTimelineSemaphore timeline(*device_);

    timeline.signal(5);
    EXPECT_THAT(timeline.getCompletedValue(), Eq(5));
    EXPECT_THAT(timeline.getScheduledValue(), Eq(5));
    EXPECT_THAT(timeline.isComplete(5), IsTrue());
    EXPECT_THAT(timeline.isComplete(6), IsFalse());

    timeline.wait(5);
}

TEST_F(VulkanTimelineSemaphoreTest, WaitTimesOutWhenUnsignaled) {
    const VulkanTimelineSemaphore timeline(*device_);

    EXPECT_THAT(timeline.wait(1, 0), IsFalse());
    EXPECT_THAT(timeline.wait(0, 0), IsTrue());
}

TEST_F(VulkanTimelineSemaphoreTest, SignaledBySubmission) {
    VulkanTimelineSemaphore timeline(*device_, 0, "Test Timeline");
    const VulkanCommandPool commandPool(
        device_->getGeneralQueue().createCommandPool(), device_->getResourceDeallocator());

    const uint64_t target = submitEmptyWorkload(*device_, commandPool, timeline);
    EXPECT_THAT(target, Eq(1));

    timeline.wait(target);
    EXPECT_THAT(timeline.getCompletedValue(), Eq(target));
}

TEST_F(VulkanTimelineSemaphoreTest, TracksSuccessiveSubmissions) {
    VulkanTimelineSemaphore timeline(*device_);
    const VulkanCommandPool commandPool(
        device_->getGeneralQueue().createCommandPool(), device_->getResourceDeallocator());

    constexpr uint64_t kSubmissionCount = 4;
    for (uint64_t i = 0; i < kSubmissionCount; ++i) {
        EXPECT_THAT(submitEmptyWorkload(*device_, commandPool, timeline), Eq(i + 1));
    }
    EXPECT_THAT(timeline.getScheduledValue(), Eq(kSubmissionCount));

    // Waiting on the last value implies every earlier one, which is what makes the per-frame fence redundant.
    timeline.wait(kSubmissionCount);
    EXPECT_THAT(timeline.isComplete(1), IsTrue());
    EXPECT_THAT(timeline.getCompletedValue(), Eq(kSubmissionCount));
}

} // namespace
} // namespace crisp
