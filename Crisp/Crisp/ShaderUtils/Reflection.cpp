#include <Crisp/ShaderUtils/Reflection.hpp>

#include <spirv_reflect.h>

#include <fstream>
#include <ranges>

namespace crisp {
namespace {
#define SPV_TO_VK_CASE_RETURN(enumEntry)                                                                               \
    case SPV_REFLECT_##enumEntry:                                                                                      \
        return VK_##enumEntry;

#define CRISP_SPV_TRY(spvFunctionCall)                                                                                 \
    if ((spvFunctionCall) != SPV_REFLECT_RESULT_SUCCESS) {                                                             \
        return resultError("Error while calling {}", #spvFunctionCall);                                                \
    }

Result<VkShaderStageFlagBits> toVulkanShaderStage(const SpvReflectShaderStageFlagBits stageBit) {
    switch (stageBit) {
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_RAYGEN_BIT_KHR)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_MISS_BIT_KHR)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_ANY_HIT_BIT_KHR)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_CALLABLE_BIT_KHR)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_INTERSECTION_BIT_KHR)

        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_COMPUTE_BIT)

        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_VERTEX_BIT)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_FRAGMENT_BIT)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_GEOMETRY_BIT)

        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_MESH_BIT_EXT)
        SPV_TO_VK_CASE_RETURN(SHADER_STAGE_TASK_BIT_EXT)
    default:
        return resultError("Failed to convert SPV Reflect shader stage {} to Vulkan!", static_cast<uint32_t>(stageBit));
    }
}

Result<VkDescriptorType> toVulkanDescriptorType(const SpvReflectDescriptorType type) {
    switch (type) {
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_SAMPLER)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_SAMPLED_IMAGE)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_STORAGE_IMAGE)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_STORAGE_BUFFER)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        SPV_TO_VK_CASE_RETURN(DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
    default:
        return resultError("Failed to convert SPV Reflect descriptor type {} to Vulkan!", static_cast<uint32_t>(type));
    }
}

Result<VkFormat> toVulkanFormat(const SpvReflectFormat format) {
    switch (format) {
        SPV_TO_VK_CASE_RETURN(FORMAT_R32G32_SFLOAT);
        SPV_TO_VK_CASE_RETURN(FORMAT_R32G32B32_SFLOAT);
        SPV_TO_VK_CASE_RETURN(FORMAT_R32G32B32A32_SFLOAT);
    default:
        return resultError("Failed to convert SPV Reflect format type {} to Vulkan!", static_cast<uint32_t>(format));
    }
}

class SpirvShaderReflectionModuleGuard {
public:
    explicit SpirvShaderReflectionModuleGuard(SpvReflectShaderModule& module)
        : m_module(module) {}

    ~SpirvShaderReflectionModuleGuard() {
        spvReflectDestroyShaderModule(&m_module);
    }

    SpirvShaderReflectionModuleGuard(const SpirvShaderReflectionModuleGuard&) = delete;
    SpirvShaderReflectionModuleGuard& operator=(const SpirvShaderReflectionModuleGuard&) = delete;

    SpirvShaderReflectionModuleGuard(SpirvShaderReflectionModuleGuard&& rhs) = delete;
    SpirvShaderReflectionModuleGuard& operator=(SpirvShaderReflectionModuleGuard&& rhs) = delete;

private:
    SpvReflectShaderModule& m_module;
};

} // namespace

void ShaderUniformInputMetadata::merge(const ShaderUniformInputMetadata& rhs) {
    // Resize current, if necessary
    if (rhs.descriptorSetLayoutBindings.size() > descriptorSetLayoutBindings.size()) {
        descriptorSetLayoutBindings.resize(rhs.descriptorSetLayoutBindings.size());
    }

    for (auto&& [setIdx, rhsLayout] : std::views::enumerate(rhs.descriptorSetLayoutBindings)) {
        // Resize layout[i], if ncessary
        if (rhsLayout.size() > descriptorSetLayoutBindings[setIdx].size()) {
            descriptorSetLayoutBindings[setIdx].resize(rhsLayout.size());
        }

        for (auto&& [bindingIdx, rhsBinding] : std::views::enumerate(rhsLayout)) {
            auto& lhsLayout = descriptorSetLayoutBindings[setIdx][bindingIdx];
            if (lhsLayout.descriptorCount > 0) {
                lhsLayout.stageFlags |= rhsBinding.stageFlags;
            }
            // If RHS binding is valid, merge it into the current LHS
            else if (rhsBinding.descriptorCount > 0) {
                lhsLayout = rhsBinding;
            }
        }
    }

    pushConstants.insert(pushConstants.end(), rhs.pushConstants.begin(), rhs.pushConstants.end());
}

Result<std::vector<char>> readSpirvFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return resultError("Failed to open spirv file: {}!", filePath.string());
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize % sizeof(uint32_t) != 0) {
        return resultError(
            "File size of {} is not divisible by 4. SPIRV code is a stream of 32-bit tokens.", filePath.string());
    }

    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

Result<ShaderVertexInputMetadata> reflectVertexMetadataFromSpirvShader(std::span<const char> spirvShader) {
    SpvReflectShaderModule module;
    CRISP_SPV_TRY(spvReflectCreateShaderModule(spirvShader.size(), spirvShader.data(), &module));
    SpirvShaderReflectionModuleGuard moduleGuard(module);

    const auto stageFlags = toVulkanShaderStage(module.shader_stage).unwrap();
    if (stageFlags != VK_SHADER_STAGE_VERTEX_BIT) {
        return resultError("Invalid shader stage {}", static_cast<uint32_t>(stageFlags));
    }

    ShaderVertexInputMetadata metadata{};
    metadata.attributes.resize(module.input_variable_count);
    for (uint32_t i = 0; i < module.input_variable_count; ++i) {
        const auto& inputVariable = module.input_variables[i]; // NOLINT
        metadata.attributes[i].format = toVulkanFormat(inputVariable->format).unwrap();
        metadata.attributes[i].location = inputVariable->location;
    }

    std::ranges::sort(metadata.attributes, [](const auto& a, const auto& b) { return a.location < b.location; });

    return metadata;
}

Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvShader(const std::span<const char> spirvShader) {
    SpvReflectShaderModule module;
    CRISP_SPV_TRY(spvReflectCreateShaderModule(spirvShader.size(), spirvShader.data(), &module));
    SpirvShaderReflectionModuleGuard moduleGuard(module);

    const auto stageFlags = toVulkanShaderStage(module.shader_stage).unwrap();

    const std::span descriptorSetSpan{
        static_cast<const SpvReflectDescriptorSet*>(module.descriptor_sets), module.descriptor_set_count};

    uint32_t totalSetCount = module.descriptor_set_count;
    for (const auto& descSet : descriptorSetSpan) {
        totalSetCount = std::max(totalSetCount, descSet.set + 1);
    }

    ShaderUniformInputMetadata metadata{};
    metadata.descriptorSetLayoutBindings.resize(totalSetCount);

    for (const auto& descSet : descriptorSetSpan) {
        const std::span bindingSpan{descSet.bindings, descSet.binding_count};

        uint32_t totalBindingCount = descSet.binding_count;
        for (const auto& binding : bindingSpan) {
            totalBindingCount = std::max(totalBindingCount, binding->binding + 1); // NOLINT
        }
        metadata.descriptorSetLayoutBindings[descSet.set].resize(totalBindingCount);

        for (const auto& spvBinding : bindingSpan) {
            auto& binding = metadata.descriptorSetLayoutBindings[descSet.set][spvBinding->binding];
            binding.binding = spvBinding->binding;
            binding.descriptorCount = spvBinding->count;
            binding.descriptorType = toVulkanDescriptorType(spvBinding->descriptor_type).unwrap();
            binding.stageFlags = stageFlags;
            binding.pImmutableSamplers = nullptr;

            // const VkDescriptorBindingFlags bindlessFlags =
            //     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
            //     VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
            // VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info{
            //     VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr};
            // extended_info.bindingCount = 1;
            // extended_info.pBindingFlags = &bindlessFlags;
        }
    }

    metadata.pushConstants.resize(module.push_constant_block_count);
    for (uint32_t i = 0; i < module.push_constant_block_count; ++i) {
        metadata.pushConstants[i].offset = module.push_constant_blocks[i].offset; // NOLINT
        metadata.pushConstants[i].size = module.push_constant_blocks[i].size;     // NOLINT
        metadata.pushConstants[i].stageFlags = stageFlags;
    }

    return metadata;
}

Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvPath(const std::filesystem::path& filePath) {
    const auto spirvFile{readSpirvFile(filePath)};
    if (!spirvFile) {
        return resultError("Failed to load spirv file from: {}", filePath.string());
    }
    return reflectUniformMetadataFromSpirvShader(*spirvFile);
}

Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvPaths(
    std::span<const std::filesystem::path> filePaths) {
    ShaderUniformInputMetadata data{};
    for (const auto& path : filePaths) {
        auto result{reflectUniformMetadataFromSpirvPath(path)};
        if (!result) {
            return resultError("Could not parse reflection data from '{}'", path.string());
        }
        data.merge(result.extract());
    }

    return data;
}

} // namespace crisp
