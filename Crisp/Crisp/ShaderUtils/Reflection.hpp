#pragma once

#include <Crisp/Core/Result.hpp>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

#include <filesystem>
#include <span>
#include <vector>

namespace crisp {
struct PipelineLayoutMetadata {
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> descriptorSetLayoutBindings;

    struct BindingInfo {
        bool isVariableArray{false};
    };

    std::vector<std::vector<BindingInfo>> bindingInfos;
    std::vector<VkPushConstantRange> pushConstants;

    void merge(const PipelineLayoutMetadata& rhs);
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
Result<PipelineLayoutMetadata> reflectPipelineLayoutFromSpirv(std::span<const char> spirvShader);
Result<PipelineLayoutMetadata> reflectPipelineLayoutFromSpirv(const std::filesystem::path& filePath);
Result<PipelineLayoutMetadata> reflectPipelineLayoutFromSpirv(std::span<const std::filesystem::path> filePaths);
} // namespace crisp
