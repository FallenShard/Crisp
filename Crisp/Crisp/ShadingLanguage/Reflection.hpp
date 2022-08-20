#pragma once

#include <Crisp/Common/Result.hpp>

#include <vulkan/vulkan.h>

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
} // namespace crisp::sl
