#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <numeric>

namespace crisp::test
{
namespace
{
using TransmittanceLutTest = VulkanTest;

using ::testing::IsNull;
using ::testing::Not;

TEST_F(TransmittanceLutTest, VulkanBuffer)
{
    constexpr VkDeviceSize elementCount = 25;
    constexpr VkDeviceSize size = sizeof(float) * elementCount;
    VulkanBuffer deviceBuffer(
        *device_,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    StagingVulkanBuffer stagingBuffer(*device_, size);
    std::vector<float> data(size / sizeof(float));
    std::iota(data.begin(), data.end(), 0.0f);
    const float* stagingPtr = stagingBuffer.getHostVisibleData<float>();
    stagingBuffer.updateFromHost(data);
    for (uint32_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(stagingPtr[i], data[i]);

    StagingVulkanBuffer downloadBuffer(*device_, deviceBuffer.getSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    {
        const ScopeCommandExecutor executor(*device_);
        auto& cmdBuffer = executor.cmdBuffer;

        deviceBuffer.copyFrom(cmdBuffer.getHandle(), stagingBuffer);
        cmdBuffer.insertBufferMemoryBarrier(
            deviceBuffer.createDescriptorInfo(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT);

        downloadBuffer.copyFrom(cmdBuffer.getHandle(), deviceBuffer);
        cmdBuffer.insertBufferMemoryBarrier(
            downloadBuffer.createDescriptorInfo(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_ACCESS_HOST_READ_BIT);
    }

    const float* ptr = downloadBuffer.getHostVisibleData<float>();
    for (uint32_t i = 0; i < elementCount; ++i)
        ASSERT_EQ(ptr[i], data[i]) << " not equal at index " << i;
}
} // namespace
} // namespace crisp::test