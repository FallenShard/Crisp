#pragma once

#include <Crisp/Core/Result.hpp>

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace crisp::sl
{
struct ShaderUniformInputMetadata
{
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> descriptorSetLayoutBindings;
    std::vector<VkPushConstantRange> pushConstants;

    void merge(ShaderUniformInputMetadata&& metadata);
};

Result<ShaderUniformInputMetadata> parseShaderUniformInputMetadata(const std::filesystem::path& sourcePath);

Result<std::vector<char>> readSpirvFile(const std::filesystem::path& filePath);
} // namespace crisp::sl
