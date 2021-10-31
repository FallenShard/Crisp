#pragma once

#include <vulkan/vulkan.h>

#include <iostream>
#include <filesystem>

namespace crisp
{
    std::string preprocess(const std::filesystem::path& resourceFolder, const std::string& shaderFilename);
}

