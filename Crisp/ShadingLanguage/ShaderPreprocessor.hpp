#pragma once

#include <iostream>
#include <filesystem>

#include <vulkan/vulkan.h>

namespace crisp
{
    std::string preprocess(const std::filesystem::path& resourceFolder, const std::string& shaderFilename);
    VkShaderModule compile(const std::string& shaderSourceCode);
}

