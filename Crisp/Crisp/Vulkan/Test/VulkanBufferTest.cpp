#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <numeric>

namespace crisp::test
{
namespace
{
using VulkanBufferTest = VulkanTest;

using ::testing::IsNull;
using ::testing::Not;

TEST_F(VulkanBufferTest, StagingVulkanBuffer)
{
    std::array<float, 100> data;
    std::iota(data.begin(), data.end(), 0.0f);

    constexpr VkDeviceSize size = data.size() * sizeof(float);
    StagingVulkanBuffer stagingBuffer(*device_, size);
    EXPECT_THAT(stagingBuffer, HandleIsValid());

    ASSERT_EQ(stagingBuffer.getSize(), size);
    stagingBuffer.updateFromHost(data);

    const float* ptr = stagingBuffer.getHostVisibleData<float>();
    for (uint32_t i = 0; i < data.size(); ++i)
        EXPECT_EQ(ptr[i], data[i]);
}

TEST_F(VulkanBufferTest, MoveConstruction)
{
    std::array<float, 100> data;
    std::iota(data.begin(), data.end(), 0.0f);

    constexpr VkDeviceSize size = data.size() * sizeof(float);
    StagingVulkanBuffer stagingBuffer(*device_, size);
    EXPECT_THAT(stagingBuffer, HandleIsValid());
    EXPECT_EQ(stagingBuffer.getSize(), size);

    const StagingVulkanBuffer another(std::move(stagingBuffer));
    EXPECT_THAT(another, HandleIsValid());
    EXPECT_EQ(another.getSize(), size);

    EXPECT_THAT(stagingBuffer, HandleIsNull());
}

TEST_F(VulkanBufferTest, VulkanBuffer)
{
    constexpr VkDeviceSize elementCount = 25;
    constexpr VkDeviceSize size = sizeof(float) * elementCount;
    std::vector<float> data(elementCount);
    std::iota(data.begin(), data.end(), 0.0f);

    VulkanBuffer deviceBuffer(
        *device_,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    StagingVulkanBuffer stagingBuffer(*device_, size);
    const float* stagingPtr = stagingBuffer.getHostVisibleData<float>();
    stagingBuffer.updateFromHost(data);
    for (uint32_t i = 0; i < data.size(); ++i)
        EXPECT_EQ(stagingPtr[i], data[i]);

    StagingVulkanBuffer downloadBuffer(*device_, deviceBuffer.getSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        ScopeCommandExecutor executor(*device_);
        const auto& cmdBuffer = executor.cmdBuffer;

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

TEST_F(VulkanBufferTest, VulkanBufferInterQueueTransfer)
{
    auto device = std::make_unique<VulkanDevice>(
        *physicalDevice_,
        createQueueConfiguration({QueueType::General, QueueType::Transfer}, *context_, *physicalDevice_),
        2);
    ASSERT_THAT(device->getGeneralQueue(), Not(HandleIs(device->getTransferQueue())));

    constexpr VkDeviceSize kElementCount = 25;
    constexpr VkDeviceSize kSize = sizeof(float) * kElementCount;
    VulkanBuffer deviceBuffer(
        *device,
        kSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    StagingVulkanBuffer stagingBuffer(*device, kSize);
    std::vector<float> data(kSize / sizeof(float));
    std::iota(data.begin(), data.end(), 0.0f);
    const float* stagingPtr = stagingBuffer.getHostVisibleData<float>();
    stagingBuffer.updateFromHost(data);
    for (uint32_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(stagingPtr[i], data[i]);

    const VulkanQueue& generalQueue = device->getGeneralQueue();
    const VulkanCommandPool commandPool(generalQueue.createCommandPool(), device->getResourceDeallocator());
    VulkanCommandBuffer cmdBuffer(commandPool.allocateCommandBuffer(*device, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    VkFence fence = device->createFence();
    cmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Copy and sync
    deviceBuffer.copyFrom(cmdBuffer.getHandle(), stagingBuffer);
    cmdBuffer.insertBufferMemoryBarrier(
        deviceBuffer.createDescriptorInfo(),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);

    // Unassigned queue family for now, until first command
    StagingVulkanBuffer downloadBuffer(*device, deviceBuffer.getSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // Transfer ownership to the transfer queue FOR DMA
    const VulkanQueue& transferQueue = device->getTransferQueue();
    cmdBuffer.transferOwnership(
        deviceBuffer.getHandle(), generalQueue.getFamilyIndex(), transferQueue.getFamilyIndex());

    // Create the transfer execution context
    const VulkanCommandPool transferPool(transferQueue.createCommandPool(), device->getResourceDeallocator());
    VulkanCommandBuffer transferCmdBuffer(transferPool.allocateCommandBuffer(*device, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    VkFence transferFence = device->createFence();
    transferCmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    downloadBuffer.copyFrom(transferCmdBuffer.getHandle(), deviceBuffer);
    transferCmdBuffer.insertBufferMemoryBarrier(
        downloadBuffer.createDescriptorInfo(),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_HOST_READ_BIT);
    transferCmdBuffer.transferOwnership(
        deviceBuffer.getHandle(), transferQueue.getFamilyIndex(), generalQueue.getFamilyIndex());
    transferCmdBuffer.end();

    StagingVulkanBuffer downloadBuffer2(*device, deviceBuffer.getSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    downloadBuffer2.copyFrom(cmdBuffer.getHandle(), deviceBuffer);
    cmdBuffer.insertBufferMemoryBarrier(
        downloadBuffer2.createDescriptorInfo(),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_HOST_READ_BIT);

    cmdBuffer.end();

    const VkSemaphore semaphore = device->createSemaphore();
    generalQueue.submit(VK_NULL_HANDLE, semaphore, cmdBuffer.getHandle(), fence);
    transferQueue.submit(
        semaphore, VK_NULL_HANDLE, transferCmdBuffer.getHandle(), transferFence, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // First wait for the general queue, it doesn't depend on anything
    // Then we can wait on the transfer queue and check if we copied correctly
    device->wait(std::array{fence, transferFence});

    vkDestroyFence(device->getHandle(), fence, nullptr);
    vkDestroyFence(device->getHandle(), transferFence, nullptr);
    vkDestroySemaphore(device->getHandle(), semaphore, nullptr);

    const float* ptr = downloadBuffer.getHostVisibleData<float>();
    const float* ptr2 = downloadBuffer2.getHostVisibleData<float>();
    for (uint32_t i = 0; i < kElementCount; ++i)
    {
        EXPECT_EQ(ptr[i], data[i]) << " not equal at index " << i;
        EXPECT_EQ(ptr2[i], data[i]) << " not equal at index " << i;
    }
}
} // namespace
} // namespace crisp::test