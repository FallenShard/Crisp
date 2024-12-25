#pragma once

#include <Crisp/Core/Result.hpp>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

#include <filesystem>
#include <string>

namespace crisp {
Result<VkShaderStageFlagBits> getShaderStageFromFilePath(const std::filesystem::path& glslShaderFilePath);
Result<VkShaderStageFlagBits> getShaderStageFromShaderType(const std::string& glslShaderType);
bool isGlslShaderExtension(const std::string& extension);
} // namespace crisp