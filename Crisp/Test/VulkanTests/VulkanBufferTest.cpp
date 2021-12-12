#include <VulkanTests/VulkanTest.hpp>

#include <Crisp/Core/Window.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>

#include <CrispCore/Result.hpp>

#include <numeric>

using namespace crisp;

class VulkanBufferTest : public VulkanTest {};

template <typename T>
struct VulkanDeviceData
{
    T deps;
    std::unique_ptr<VulkanDevice> device;
};

auto createDevice()
{
    struct
    {
        std::unique_ptr<Window> window{ std::make_unique<Window>(0, 0, 200, 200, "unit_test", true) };
        std::unique_ptr<VulkanContext> context{ std::make_unique<VulkanContext>(window->createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanExtensions(), true) };
        std::unique_ptr<VulkanPhysicalDevice> physicalDevice{ std::make_unique<VulkanPhysicalDevice>(context->selectPhysicalDevice(createDefaultDeviceExtensions()).unwrap()) };
    } deps;

    auto device = std::make_unique<VulkanDevice>(*deps.physicalDevice, createDefaultQueueConfiguration(*deps.context, *deps.physicalDevice), 2);
    return VulkanDeviceData{ std::move(deps), std::move(device) };
}

class ScopeCommandBufferExecutor
{
public:
    ScopeCommandBufferExecutor(const VulkanDevice& device)
        : device(device)
        , commandPool(device.getGeneralQueue().createCommandPool(0), device.getResourceDeallocator())
        , cmdBuffer(commandPool.allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
        , fence(device.createFence(0))
    {
        cmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    ~ScopeCommandBufferExecutor()
    {
        cmdBuffer.end();
        device.getGeneralQueue().submit(cmdBuffer.getHandle(), fence);
        device.wait(fence);
        vkDestroyFence(device.getHandle(), fence, nullptr);
    }

    ScopeCommandBufferExecutor(ScopeCommandBufferExecutor&&) = delete;
    ScopeCommandBufferExecutor(const ScopeCommandBufferExecutor&) = delete;

    const VulkanDevice& device;
    const VulkanCommandPool commandPool;
    VulkanCommandBuffer cmdBuffer;
    VkFence fence;
};

TEST_F(VulkanBufferTest, StagingVulkanBuffer)
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
    const float* ptr =stagingBuffer.getHostVisibleData<float>();
    for (uint32_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(ptr[i], data[i]);
}

TEST_F(VulkanBufferTest, VulkanBuffer)
{
    const auto& [deps, device] = createDevice();

    constexpr VkDeviceSize elementCount = 25;
    constexpr VkDeviceSize size = sizeof(float) * elementCount;
    VulkanBuffer deviceBuffer(*device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    StagingVulkanBuffer stagingBuffer(*device, size);
    std::vector<float> data(size / sizeof(float));
    std::iota(data.begin(), data.end(), 0.0f);
    const float* stagingPtr = stagingBuffer.getHostVisibleData<float>();
    stagingBuffer.updateFromHost(data);
    for (uint32_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(stagingPtr[i], data[i]);

    StagingVulkanBuffer downloadBuffer(*device, deviceBuffer.getSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    {
        ScopeCommandBufferExecutor executor(*device);
        auto& cmdBuffer = executor.cmdBuffer;

        deviceBuffer.copyFrom(cmdBuffer.getHandle(), stagingBuffer);
        cmdBuffer.insertPipelineBarrier(deviceBuffer.createView(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        downloadBuffer.copyFrom(cmdBuffer.getHandle(), deviceBuffer);
        cmdBuffer.insertPipelineBarrier(downloadBuffer.createView(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
    }

    const float* ptr = downloadBuffer.getHostVisibleData<float>();
    for (uint32_t i = 0; i < elementCount; ++i)
        ASSERT_EQ(ptr[i], data[i]) << " not equal at index " << i;
}


TEST_F(VulkanBufferTest, VulkanBufferInterQueueTransfer)
{
    const auto& [deps, device] = createDevice();

    constexpr VkDeviceSize elementCount = 25;
    constexpr VkDeviceSize size = sizeof(float) * elementCount;
    VulkanBuffer deviceBuffer(*device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    StagingVulkanBuffer stagingBuffer(*device, size);
    std::vector<float> data(size / sizeof(float));
    std::iota(data.begin(), data.end(), 0.0f);
    const float* stagingPtr = stagingBuffer.getHostVisibleData<float>();
    stagingBuffer.updateFromHost(data);
    for (uint32_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(stagingPtr[i], data[i]);

    
    const VulkanQueue& generalQueue = device->getGeneralQueue();
    const VulkanCommandPool commandPool(generalQueue.createCommandPool(0), device->getResourceDeallocator());
    VulkanCommandBuffer cmdBuffer(commandPool.allocateCommandBuffer(*device, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    VkFence fence = device->createFence(0);
    cmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Copy and sync
    deviceBuffer.copyFrom(cmdBuffer.getHandle(), stagingBuffer);
    cmdBuffer.insertPipelineBarrier(deviceBuffer.createView(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    
    // Unassigned queue family for now, until first command
    StagingVulkanBuffer downloadBuffer(*device, deviceBuffer.getSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    VkSemaphore semaphore = device->createSemaphore();

    // Transfer ownership to the transfer queue FOR DMA
    const VulkanQueue& transferQueue = device->getTransferQueue();
    cmdBuffer.transferOwnership(deviceBuffer.getHandle(), generalQueue.getFamilyIndex(), transferQueue.getFamilyIndex());

    // Create the transfer execution context
    const VulkanCommandPool transferPool(transferQueue.createCommandPool(0), device->getResourceDeallocator());
    VulkanCommandBuffer transferCmdBuffer(transferPool.allocateCommandBuffer(*device, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    VkFence transferFence = device->createFence(0);
    transferCmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    downloadBuffer.copyFrom(transferCmdBuffer.getHandle(), deviceBuffer);
    transferCmdBuffer.insertPipelineBarrier(downloadBuffer.createView(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
    transferCmdBuffer.transferOwnership(deviceBuffer.getHandle(), transferQueue.getFamilyIndex(), generalQueue.getFamilyIndex());
    transferCmdBuffer.end();
    
    
    StagingVulkanBuffer downloadBuffer2(*device, deviceBuffer.getSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    downloadBuffer2.copyFrom(cmdBuffer.getHandle(), deviceBuffer);
    cmdBuffer.insertPipelineBarrier(downloadBuffer2.createView(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);

    cmdBuffer.end();
    generalQueue.submit(VK_NULL_HANDLE, semaphore, cmdBuffer.getHandle(), fence);
    transferQueue.submit(semaphore, VK_NULL_HANDLE, transferCmdBuffer.getHandle(), transferFence, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // First wait for the general queue, it doesn't depend on anything
    // Then we can wait on the transfer queue and check if we copied correctly
    device->wait(std::array{ fence, transferFence });

    vkDestroyFence(device->getHandle(), fence, nullptr);
    vkDestroyFence(device->getHandle(), transferFence, nullptr);

    const float* ptr = downloadBuffer.getHostVisibleData<float>();
    const float* ptr2 = downloadBuffer2.getHostVisibleData<float>();
    for (uint32_t i = 0; i < elementCount; ++i)
    {
        EXPECT_EQ(ptr[i], data[i]) << " not equal at index " << i;
        EXPECT_EQ(ptr2[i], data[i]) << " not equal at index " << i;
    }

    vkDestroySemaphore(device->getHandle(), semaphore, nullptr);
}