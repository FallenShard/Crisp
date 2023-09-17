#pragma once

#include <Crisp/Core/Result.hpp>

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <filesystem>
#include <span>
#include <vector>

namespace crisp
{
struct ShaderUniformInputMetadata
{
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> descriptorSetLayoutBindings;
    std::vector<VkPushConstantRange> pushConstants;

    void merge(ShaderUniformInputMetadata&& rhs);
};

Result<std::vector<char>> readSpirvFile(const std::filesystem::path& filePath);

Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvPath(const std::filesystem::path& filePath);
Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvShader(std::span<const char> spirvShader);
} // namespace crisp
