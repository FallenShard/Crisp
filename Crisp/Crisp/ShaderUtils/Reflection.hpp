#pragma once

#include <Crisp/Core/Result.hpp>

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <filesystem>
#include <span>
#include <vector>

namespace crisp {
struct ShaderUniformInputMetadata {
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> descriptorSetLayoutBindings;
    std::vector<VkPushConstantRange> pushConstants;

    void merge(const ShaderUniformInputMetadata& rhs);
};

struct ShaderVertexInputMetadata {
    struct VertexAttributeDescription {
        VkFormat format;
        uint32_t location;
    };

    std::vector<VertexAttributeDescription> attributes;
};

Result<std::vector<char>> readSpirvFile(const std::filesystem::path& filePath);

Result<ShaderVertexInputMetadata> reflectVertexMetadataFromSpirvShader(std::span<const char> spirvShader);
Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvShader(std::span<const char> spirvShader);
Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvPath(const std::filesystem::path& filePath);
Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvPaths(
    std::span<const std::filesystem::path> filePaths);
} // namespace crisp
