#include <Crisp/Vulkan/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanMemoryAllocatorTest = VulkanTest;

TEST_F(VulkanMemoryAllocatorTest, EmptyMemoryUsage) {
    const auto memoryMetrics = device_->getMemoryAllocator().getDeviceMemoryUsage();
    ASSERT_EQ(memoryMetrics.bufferMemoryUsed, 0);
    ASSERT_EQ(memoryMetrics.imageMemoryUsed, 0);
    ASSERT_EQ(memoryMetrics.stagingMemoryUsed, 0);
}
} // namespace
} // namespace crisp