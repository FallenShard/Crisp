#pragma once

#include <string>

#include <vulkan/vulkan.h>

namespace crisp
{
    class ShaderLoader
    {
    public:
        VkShaderModule load(VkDevice device, const std::string& vulkanShaderFile);
    };
}