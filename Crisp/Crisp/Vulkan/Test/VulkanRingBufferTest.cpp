#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

#include <numeric>

#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanRingBufferTest = VulkanTest;

using ::testing::ElementsAreArray;

TEST_F(VulkanRingBufferTest, Updates) {
    constexpr uint32_t kElementCount{100};
    std::array<float, kElementCount> data{};
    std::iota(data.begin(), data.end(), 0.0f);

    constexpr VkDeviceSize byteSize = data.size() * sizeof(float);
    VulkanRingBuffer buffer(
        device_.get(), VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, byteSize, data.data());
    EXPECT_THAT(buffer, HandleIsValid());
    EXPECT_THAT(buffer.getDeviceBuffer().getHandle(), buffer.getHandle());
    EXPECT_THAT(toStdVec<float>(buffer.getDeviceBuffer()), ElementsAreArray(data));

    auto data2 = data;
    for (auto& v : data2) {
        v *= 2;
    }
    buffer.updateStagingBuffer({.data = data2.data(), .size = data2.size() * sizeof(float)}, 1);
    device_->getGeneralQueue().submitAndWait([&buffer](const VkCommandBuffer cmdBuffer) {
        buffer.updateDeviceBuffer(cmdBuffer);
    });
    EXPECT_THAT(toStdVec<float>(buffer.getDeviceBuffer()), ElementsAreArray(data2));
}

} // namespace
} // namespace crisp