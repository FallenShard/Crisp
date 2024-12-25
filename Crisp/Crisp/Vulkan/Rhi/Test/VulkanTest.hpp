#pragma once

#include <gmock/gmock.h>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Core/Result.hpp>
#include <Crisp/Core/Test/ResultTestUtils.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanInstance.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

namespace crisp {
MATCHER(HandleIsValid, "Checks whether the handle is not null.") {
    std::ignore = result_listener;
    return arg.getHandle() != VK_NULL_HANDLE;
}

MATCHER(HandleIsNull, "Checks whether the handle is null.") {
    std::ignore = result_listener;
    *result_listener << "Arg is not null!";
    return arg.getHandle() == VK_NULL_HANDLE;
}

MATCHER_P(HandleIs, rhs, "Checks whether the handle is equal to another object's handle.") {
    *result_listener << "Expected: " << rhs.getHandle() << "\n";
    *result_listener << "Actual:   " << arg.getHandle() << "\n";
    return arg.getHandle() == rhs.getHandle();
}

enum class SurfacePolicy { Headless, HiddenWindow };

template <SurfacePolicy surfacePolicy = SurfacePolicy::Headless>
class VulkanTestBase : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        spdlog::set_level(spdlog::level::warn);
        glfwInit();

        if constexpr (surfacePolicy == SurfacePolicy::HiddenWindow) {
            window_ = std::make_unique<Window>(
                glm::ivec2{0, 0}, glm::ivec2{kDefaultWidth, kDefaultHeight}, "unit_test", WindowVisibility::Hidden);
        }

        instance_ = std::make_unique<VulkanInstance>(
            surfacePolicy == SurfacePolicy::HiddenWindow ? window_->createSurfaceCallback() : nullptr,
            ApplicationEnvironment::getRequiredVulkanInstanceExtensions(),
            true);
        physicalDevice_ = std::make_unique<VulkanPhysicalDevice>(
            selectPhysicalDevice(*instance_, createDefaultDeviceExtensions()).unwrap());
        device_ = std::make_unique<VulkanDevice>(
            *physicalDevice_,
            surfacePolicy == SurfacePolicy::HiddenWindow
                ? createDefaultQueueConfiguration(*instance_, *physicalDevice_)
                : createQueueConfiguration({QueueType::General}, *instance_, *physicalDevice_),
            kRendererVirtualFrameCount);
    }

    static void TearDownTestSuite() {
        device_.reset();
        physicalDevice_.reset();
        instance_.reset();

        glfwTerminate();
    }

    void SetUp() override {}

    void TearDown() override {
        device_->getResourceDeallocator().freeAllResources();
    }

    inline static constexpr uint32_t kDefaultWidth = 200;
    inline static constexpr uint32_t kDefaultHeight = 200;

    inline static std::unique_ptr<Window> window_;
    inline static std::unique_ptr<VulkanInstance> instance_;
    inline static std::unique_ptr<VulkanPhysicalDevice> physicalDevice_;
    inline static std::unique_ptr<VulkanDevice> device_;
};

using VulkanTest = VulkanTestBase<SurfacePolicy::Headless>;
using VulkanTestWithSurface = VulkanTestBase<SurfacePolicy::HiddenWindow>;

struct ScopeCommandExecutor {
    inline explicit ScopeCommandExecutor(const VulkanDevice& device)
        : device(device)
        , commandPool(device.getGeneralQueue().createCommandPool(), device.getResourceDeallocator())
        , cmdBuffer(commandPool.allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
        , fence(device.createFence(0)) {
        cmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    inline ~ScopeCommandExecutor() {
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

} // namespace crisp