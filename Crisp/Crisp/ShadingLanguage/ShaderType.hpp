#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Result.hpp>

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <filesystem>
#include <string>

namespace crisp
{
Result<VkShaderStageFlags> getShaderStageFromFilePath(const std::filesystem::path& glslShaderFilePath);
Result<VkShaderStageFlags> getShaderStageFromShaderType(const std::string& glslShaderType);
} // namespace crisp