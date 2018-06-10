#pragma once

#include <CrispCore/Log.hpp>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace crisp
{
    class ApplicationEnvironment
    {
    public:
        ApplicationEnvironment(int argc, char** argv);
        ~ApplicationEnvironment();
    };
}