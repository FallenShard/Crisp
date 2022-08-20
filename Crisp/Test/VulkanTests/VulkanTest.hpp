#pragma once

#include <gtest/gtest.h>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <Crisp/Common/Result.hpp>

#include <glfw/glfw3.h>
#include <spdlog/spdlog.h>

class VulkanTest : public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
        spdlog::set_level(spdlog::level::warn);
        glfwInit();
    }

    static void TearDownTestCase()
    {
        glfwTerminate();
    }
};

template <typename T, auto deleter>
class UniqueHandleWrapper
{
public:
    inline UniqueHandleWrapper(T handle)
        : handle(handle)
    {
    }

    ~UniqueHandleWrapper()
    {
        deleter(handle);
    }

    UniqueHandleWrapper(const UniqueHandleWrapper&) = delete;

    inline UniqueHandleWrapper(UniqueHandleWrapper&& rhs) noexcept
        : handle(std::exchange(rhs.handle, nullptr))
    {
    }

    UniqueHandleWrapper& operator=(const UniqueHandleWrapper&) = delete;

    inline UniqueHandleWrapper& operator=(UniqueHandleWrapper&& rhs) noexcept
    {
        handle = std::exchange(rhs.handle, nullptr);
    }

    operator T() const
    {
        return handle;
    }

private:
    T handle;
};

template <typename T>
struct VulkanDeviceData
{
    T deps;
    std::unique_ptr<crisp::VulkanDevice> device;
};

inline auto createDevice()
{
    struct
    {
        std::unique_ptr<crisp::Window> window{std::make_unique<crisp::Window>(0, 0, 200, 200, "unit_test", true)};
        std::unique_ptr<crisp::VulkanContext> context{std::make_unique<crisp::VulkanContext>(
            window->createSurfaceCallback(),
            crisp::ApplicationEnvironment::getRequiredVulkanInstanceExtensions(),
            true)};
        std::unique_ptr<crisp::VulkanPhysicalDevice> physicalDevice{std::make_unique<crisp::VulkanPhysicalDevice>(
            context->selectPhysicalDevice(crisp::createDefaultDeviceExtensions()).unwrap())};
    } deps;

    auto device = std::make_unique<crisp::VulkanDevice>(
        *deps.physicalDevice, crisp::createDefaultQueueConfiguration(*deps.context, *deps.physicalDevice), 2);
    return VulkanDeviceData{std::move(deps), std::move(device)};
}