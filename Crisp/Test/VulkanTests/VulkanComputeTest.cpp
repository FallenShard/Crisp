#include <Test/VulkanTests/ScopeCommandBufferExecutor.hpp>
#include <Test/VulkanTests/VulkanTest.hpp>

#include <numeric>

using namespace crisp;

class VulkanComputeTest : public VulkanTest
{
};

TEST_F(VulkanComputeTest, StagingVulkanBuffer)
{
    const auto& [deps, device] = createDevice();

    constexpr VkDeviceSize size = 100;
    StagingVulkanBuffer stagingBuffer(*device, size);
    ASSERT_TRUE(stagingBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT_EQ(stagingBuffer.getSize(), size);

    std::vector<float> data(size / sizeof(float));
    for (uint32_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(i);

    stagingBuffer.updateFromHost(data);
    const float* ptr = stagingBuffer.getHostVisibleData<float>();
    for (uint32_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(ptr[i], data[i]);
}

TEST_F(VulkanComputeTest, VulkanBuffer)
{
    /*auto node = createComputeNode();
    node->setInputBuffer(0, 0, camerBuffer);
    node->setInputBuffer(0, 1, params);
    node->setOutputBuffer(1, 0, particles);
    node->*/
}