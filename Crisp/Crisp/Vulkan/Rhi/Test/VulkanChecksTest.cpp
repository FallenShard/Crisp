#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

#include <gmock/gmock.h>

using ::testing::AllOf;
using ::testing::HasSubstr;

namespace {

// Death tests match against the child's stderr, but the default logger writes to stdout.
void routeLogsToStderr() {
    spdlog::drop_all();
    spdlog::set_default_logger(spdlog::stderr_color_st("vulkan-checks-test"));
    spdlog::set_pattern("%v");
}

VkResult failingCall() {
    return VK_ERROR_DEVICE_LOST;
}

VkResult succeedingCall() {
    return VK_SUCCESS;
}

int g_calls = 0;

VkResult countedCall() {
    ++g_calls;
    return VK_SUCCESS;
}

TEST(VulkanChecksTest, PassesThroughASuccessfulCall) {
    VK_FATAL(succeedingCall());
    SUCCEED();
}

TEST(VulkanChecksTest, ReportsTheFailingExpressionAndResult) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            VK_FATAL(failingCall());
        },
        AllOf(HasSubstr("failingCall()"), HasSubstr("VK_ERROR_DEVICE_LOST")));
}

TEST(VulkanChecksTest, ReportsTheFormattedMessage) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            VK_FATAL(failingCall(), "while creating {} buffers", 3);
        },
        HasSubstr("while creating 3 buffers"));
}

TEST(VulkanChecksTest, EvaluatesTheCallExactlyOnce) {
    g_calls = 0;
    VK_FATAL(countedCall());
    EXPECT_EQ(g_calls, 1);
}

// The call is the Vulkan work itself, so it must survive into release even though the check does not.
TEST(VulkanChecksTest, DevCheckEvaluatesTheCallInEveryConfiguration) {
    g_calls = 0;
    VK_DEV_CHECK(countedCall());
    EXPECT_EQ(g_calls, 1);

    VK_DEV_CHECK(countedCall(), "message args stay type checked in {}", "release");
    EXPECT_EQ(g_calls, 2);
}

} // namespace
