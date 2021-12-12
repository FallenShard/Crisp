#include <VulkanTests/VulkanTest.hpp>

#include <Crisp/Core/Window.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>

#include <CrispCore/Result.hpp>

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
        std::unique_ptr<VulkanContext> context{ std::make_unique<VulkanContext>(window->createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanExtensions(), false) };
        std::unique_ptr<VulkanPhysicalDevice> physicalDevice{ std::make_unique<VulkanPhysicalDevice>(context->selectPhysicalDevice(createDefaultDeviceExtensions()).unwrap()) };
    } deps;

    auto device = std::make_unique<VulkanDevice>(*deps.physicalDevice, createDefaultQueueConfiguration(*deps.context, *deps.physicalDevice), 2);
    return VulkanDeviceData{ std::move(deps), std::move(device) };
}

TEST_F(VulkanBufferTest, StagingVulkanBuffer)
{
    const auto& [deps, device] = createDevice();

    const VkDeviceSize size = 100;
    StagingVulkanBuffer stagingBuffer(*device, size);
    ASSERT_TRUE(stagingBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT_EQ(stagingBuffer.getSize(), size);
}
