#pragma once

#include <CrispCore/RobinHood.hpp>
#include <CrispCore/Result.hpp>

#include <vulkan/vulkan.h>

#include <filesystem>
#include <string>

namespace crisp
{
	Result<VkShaderStageFlags> getShaderStageFromFilePath(const std::filesystem::path& glslShaderFilePath);
	Result<VkShaderStageFlags> getShaderStageFromShaderType(const std::string& glslShaderType);
}