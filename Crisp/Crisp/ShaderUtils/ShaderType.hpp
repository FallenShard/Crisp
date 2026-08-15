#pragma once

#include <Crisp/Core/Result.hpp>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace crisp {
Result<VkShaderStageFlagBits> getShaderStageFromFilePath(const std::filesystem::path& shaderFilePath);
Result<VkShaderStageFlagBits> getShaderStageFromShaderType(const std::string& glslShaderType);
bool isGlslShaderExtension(const std::string& extension);
bool isGlslShaderExtension(std::string_view extension);
} // namespace crisp
