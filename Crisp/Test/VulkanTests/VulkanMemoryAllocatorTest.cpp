#include <VulkanTests/VulkanTest.hpp>

#include <Crisp/Core/Window.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanMemoryAllocator.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <CrispCore/Result.hpp>
#include <glfw/glfw3.h>

using namespace crisp;

class VulkanMemoryAllocatorTest : public VulkanTest {};

template <typename T, auto deleter>
struct UniqueHandleWrapper
{
    inline UniqueHandleWrapper(VkDevice handle) : handle(handle) {}
    ~UniqueHandleWrapper() { deleter(handle); }

    UniqueHandleWrapper(const UniqueHandleWrapper&) = delete;
    inline UniqueHandleWrapper(UniqueHandleWrapper&& rhs) : handle(std::exchange(rhs.handle, nullptr)) {}
    UniqueHandleWrapper& operator=(const UniqueHandleWrapper&) = delete;
    inline UniqueHandleWrapper& operator=(UniqueHandleWrapper&& rhs) { handle = std::exchange(rhs.handle, nullptr); }

    T handle;
};

using UniqueDeviceWrapper = UniqueHandleWrapper<VkDevice, [](VkDevice device) { vkDestroyDevice(device, nullptr); }>;

auto createMemoryAllocator()
{
    struct {
        VulkanContext context{ nullptr, {}, false };
        VulkanPhysicalDevice physicalDevice{ context.selectPhysicalDevice({}).unwrap() };
        UniqueDeviceWrapper device{physicalDevice.createLogicalDevice(createDefaultQueueConfiguration(context, physicalDevice)) };
    } dependencies;

    VulkanMemoryAllocator physicalDevice(dependencies.physicalDevice, dependencies.device.handle);
    return std::make_pair(std::move(physicalDevice), std::move(dependencies));
}

TEST_F(VulkanMemoryAllocatorTest, HeapsPresent)
{
    const auto& [allocator, deps] = createMemoryAllocator();
    ASSERT_NE(allocator.getDeviceBufferHeap(), nullptr);
    ASSERT_NE(allocator.getDeviceImageHeap(), nullptr);
    ASSERT_NE(allocator.getStagingBufferHeap(), nullptr);
}

TEST_F(VulkanMemoryAllocatorTest, EmptyMemoryUsage)
{
    const auto& [allocator, deps] = createMemoryAllocator();
    const auto memoryMetrics = allocator.getDeviceMemoryUsage();
    ASSERT_EQ(memoryMetrics.bufferMemoryUsed, 0);
    ASSERT_EQ(memoryMetrics.imageMemoryUsed, 0);
    ASSERT_EQ(memoryMetrics.stagingMemoryUsed, 0);
}
