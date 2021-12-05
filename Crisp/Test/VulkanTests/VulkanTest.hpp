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