#pragma once

#include <gmock/gmock.h>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Core/Result.hpp>
#include <Crisp/Core/Test/ResultTestUtils.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <glfw/glfw3.h>

namespace crisp::test
{
MATCHER(HandleIsValid, "Checks whether the handle is not null.")
{
    std::ignore = result_listener;
    return arg.getHandle() != VK_NULL_HANDLE;
}

MATCHER(HandleIsNull, "Checks whether the handle is null.")
{
    std::ignore = result_listener;
    *result_listener << "Arg is not null!";
    return arg.getHandle() == VK_NULL_HANDLE;
}

MATCHER_P(HandleIs, rhs, "Checks whether the handle is equal to another object's handle.")
{
    *result_listener << "Expected: " << rhs.getHandle() << "\n";
    *result_listener << "Actual:   " << arg.getHandle() << "\n";
    return arg.getHandle() == rhs.getHandle();
}

class VulkanTest : public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
        spdlog::set_level(spdlog::level::warn);
        glfwInit();

        static constexpr uint32_t virtualFrameCount = 2;

        context_ = std::make_unique<VulkanContext>(
            nullptr, ApplicationEnvironment::getRequiredVulkanInstanceExtensions(), true);
        physicalDevice_ = std::make_unique<VulkanPhysicalDevice>(
            context_->selectPhysicalDevice(createDefaultDeviceExtensions()).unwrap());
        device_ = std::make_unique<VulkanDevice>(
            *physicalDevice_,
            createQueueConfiguration({QueueType::General}, *context_, *physicalDevice_),
            virtualFrameCount);
    }

    static void TearDownTestCase()
    {
        device_.reset();
        physicalDevice_.reset();
        context_.reset();

        glfwTerminate();
    }

    inline static std::unique_ptr<VulkanContext> context_;
    inline static std::unique_ptr<VulkanPhysicalDevice> physicalDevice_;
    inline static std::unique_ptr<VulkanDevice> device_;
};

class ScopeCommandExecutor
{
public:
    inline ScopeCommandExecutor(const VulkanDevice& device)
        : device(device)
        , commandPool(device.getGeneralQueue().createCommandPool(), device.getResourceDeallocator())
        , cmdBuffer(commandPool.allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
        , fence(device.createFence(0))
    {
        cmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    inline ~ScopeCommandExecutor()
    {
        cmdBuffer.end();
        device.getGeneralQueue().submit(cmdBuffer.getHandle(), fence);
        device.wait(fence);
        vkDestroyFence(device.getHandle(), fence, nullptr);
    }

    ScopeCommandExecutor(const ScopeCommandExecutor&) = delete;
    ScopeCommandExecutor& operator=(const ScopeCommandExecutor&) = delete;

    ScopeCommandExecutor(ScopeCommandExecutor&&) = delete;
    ScopeCommandExecutor& operator=(ScopeCommandExecutor&&) = delete;

    const VulkanDevice& device;
    const VulkanCommandPool commandPool;
    VulkanCommandBuffer cmdBuffer;
    VkFence fence;
};

} // namespace crisp::test