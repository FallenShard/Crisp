#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanMemoryAllocator.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <Crisp/Core/Result.hpp>
#include <glfw/glfw3.h>

using namespace crisp;

class VulkanMemoryAllocatorTest : public VulkanTest
{
};

using UniqueDeviceWrapper = UniqueHandleWrapper<
    VkDevice,
    [](VkDevice device)
    {
        vkDestroyDevice(device, nullptr);
    }>;

auto createMemoryAllocator()
{
    struct
    {
        std::unique_ptr<VulkanContext> context{
            std::make_unique<VulkanContext>(nullptr, std::vector<std::string>{}, false)};
        VulkanPhysicalDevice physicalDevice{context->selectPhysicalDevice({}).unwrap()};
        UniqueDeviceWrapper device{
            createLogicalDeviceHandle(physicalDevice, createDefaultQueueConfiguration(*context, physicalDevice))};
    } dependencies;

    VulkanMemoryAllocator memoryAllocator(dependencies.physicalDevice, dependencies.device);
    return std::make_pair(std::move(memoryAllocator), std::move(dependencies));
}

TEST_F(VulkanMemoryAllocatorTest, EmptyMemoryUsage)
{
    const auto& [allocator, deps] = createMemoryAllocator();
    const auto memoryMetrics = allocator.getDeviceMemoryUsage();
    ASSERT_EQ(memoryMetrics.bufferMemoryUsed, 0);
    ASSERT_EQ(memoryMetrics.imageMemoryUsed, 0);
    ASSERT_EQ(memoryMetrics.stagingMemoryUsed, 0);
}
