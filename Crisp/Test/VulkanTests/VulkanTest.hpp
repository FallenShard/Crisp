#pragma once

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <glfw/glfw3.h>

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
    inline UniqueHandleWrapper(T handle) : handle(handle) {}
    ~UniqueHandleWrapper() { deleter(handle); }

    UniqueHandleWrapper(const UniqueHandleWrapper&) = delete;
    inline UniqueHandleWrapper(UniqueHandleWrapper&& rhs) : handle(std::exchange(rhs.handle, nullptr)) {}
    UniqueHandleWrapper& operator=(const UniqueHandleWrapper&) = delete;
    inline UniqueHandleWrapper& operator=(UniqueHandleWrapper&& rhs) { handle = std::exchange(rhs.handle, nullptr); }

    operator T() const { return handle; }

private:
    T handle;
};