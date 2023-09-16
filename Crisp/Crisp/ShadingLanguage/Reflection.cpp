#include <Crisp/ShadingLanguage/Reflection.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/IO/FileUtils.hpp>
#include <Crisp/ShadingLanguage/Lexer.hpp>
#include <Crisp/ShadingLanguage/Parser.hpp>
#include <Crisp/ShadingLanguage/ShaderType.hpp>
#include <Crisp/Utils/Enumerate.hpp>

#include <spirv_reflect.h>

#include <fstream>
#include <optional>

namespace crisp::sl
{
namespace
{
uint32_t getPushConstantFieldOffset(const StructFieldDeclaration& field)
{
    for (const auto& qualif : field.fullType->qualifiers)
    {
        if (qualif->qualifier.type == crisp::sl::TokenType::Layout)
        {
            auto layoutQualifier = dynamic_cast<crisp::sl::LayoutQualifier*>(qualif.get());
            if (!layoutQualifier || layoutQualifier->ids.empty())
            {
                return 0;
            }

            if (auto bin = dynamic_cast<BinaryExpr*>(layoutQualifier->ids[0].get()))
            {
                auto* left = dynamic_cast<Variable*>(bin->left.get());
                auto* right = dynamic_cast<Literal*>(bin->right.get());
                if (left && right && left->name.lexeme == "offset")
                {
                    return std::any_cast<int>(right->value);
                }
            }
        }
    }

    return 0;
}

uint32_t getPushConstantFieldArraySize(const StructFieldDeclaration& field)
{
    if (field.variable->arraySpecifiers.empty())
    {
        return 1;
    }
    else if (auto e = dynamic_cast<Literal*>(field.variable->arraySpecifiers[0]->expr.get()))
    {
        return std::any_cast<int>(e->value);
    }

    return 1;
}

VkPushConstantRange parsePushConstant(const Statement* statement, VkShaderStageFlags stage)
{
    VkPushConstantRange pc = {};
    pc.stageFlags = stage;
    pc.offset = 0xFFFFFFFF;

    const auto* block = dynamic_cast<const BlockDeclaration*>(statement);
    if (block)
    {
        for (const auto& field : block->fields)
        {
            auto tokenType = field->fullType->specifier->type.type;
            pc.offset = std::min(pc.offset, getPushConstantFieldOffset(*field));
            uint32_t multiplier = getPushConstantFieldArraySize(*field);
            uint32_t fieldSize = 0;
            switch (tokenType)
            {
            case TokenType::Mat4:
                fieldSize = 16 * sizeof(float);
                break;
            case TokenType::Mat3:
                fieldSize = 9 * sizeof(float);
                break;
            case TokenType::Vec4:
                fieldSize = 4 * sizeof(float);
                break;
            case TokenType::Vec3:
                fieldSize = 3 * sizeof(float);
                break;
            case TokenType::Vec2:
                fieldSize = 2 * sizeof(float);
                break;
            case TokenType::Float:
                fieldSize = sizeof(float);
                break;
            case TokenType::Uvec3:
                fieldSize = 3 * sizeof(uint32_t);
                break;
            case TokenType::Uint:
                fieldSize = sizeof(uint32_t);
                break;
            case TokenType::Int:
                fieldSize = sizeof(int32_t);
                break;

            default:
                spdlog::critical(
                    "Unknown token size '{}' while parsing push constant!", field->fullType->specifier->type.lexeme);
                break;
            }
            pc.size += fieldSize * multiplier;
        }
    }

    return pc;
}
} // namespace

Result<ShaderUniformInputMetadata> parseShaderUniformInputMetadata(const std::filesystem::path& sourcePath)
{
    const auto stageFlags = getShaderStageFromFilePath(sourcePath).unwrap();
    auto tokens = sl::Lexer(fileToString(sourcePath).unwrap()).scanTokens();
    const auto statements = sl::Parser(std::move(tokens)).parse();

    ShaderUniformInputMetadata metadata{};
    for (const auto& statement : statements)
    {
        std::optional<uint32_t> setId;
        VkDescriptorSetLayoutBinding binding{};
        binding.descriptorCount = 1;
        binding.stageFlags = stageFlags;

        auto parseLayoutQualifier =
            [&metadata, &binding, &statement, &setId](const sl::LayoutQualifier& layoutQualifier)
        {
            for (const auto& id : layoutQualifier.ids)
            {
                if (const auto bin = dynamic_cast<sl::BinaryExpr*>(id.get()))
                {
                    const auto* left = dynamic_cast<sl::Variable*>(bin->left.get());
                    const auto* right = dynamic_cast<sl::Literal*>(bin->right.get());
                    if (left && right)
                    {
                        if (left->name.lexeme == "set")
                        {
                            setId = std::any_cast<int32_t>(right->value);
                        }
                        else if (left->name.lexeme == "binding")
                        {
                            binding.binding = std::any_cast<int32_t>(right->value);
                        }
                    }
                }
                else if (const auto identifier = dynamic_cast<sl::Variable*>(id.get()))
                {
                    if (identifier->name.lexeme == "push_constant")
                    {
                        metadata.pushConstants.push_back(parsePushConstant(statement.get(), binding.stageFlags));
                    }
                }
            }
        };

        if (auto initList = dynamic_cast<sl::InitDeclaratorList*>(statement.get()))
        {
            for (auto& qualifier : initList->fullType->qualifiers)
            {
                if (qualifier->qualifier.type == sl::TokenType::Layout)
                {
                    parseLayoutQualifier(*dynamic_cast<sl::LayoutQualifier*>(qualifier.get()));
                }
            }

            if (!initList->fullType->specifier)
            {
                continue;
            }

            auto typeLexeme = initList->fullType->specifier->type.lexeme;
            if (typeLexeme == "sampler" || typeLexeme == "samplerShadow")
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            }
            else if (typeLexeme.find("sampler") != std::string::npos)
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
            else if (typeLexeme.find("texture") != std::string::npos)
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            }
            else if (typeLexeme.find("image") != std::string::npos)
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            }
            else if (typeLexeme.find("textureBuffer") != std::string::npos)
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            }
            else if (typeLexeme.find("imageBuffer") != std::string::npos)
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            }
            else if (typeLexeme.find("subpassInput") != std::string::npos)
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            }
        }
        else if (auto block = dynamic_cast<sl::BlockDeclaration*>(statement.get()))
        {
            for (uint32_t i = 0; i < block->qualifiers.size(); ++i)
            {
                const auto& qualifier = block->qualifiers[i];

                if (qualifier->qualifier.type == sl::TokenType::Layout)
                {
                    auto layoutQualifier = dynamic_cast<sl::LayoutQualifier*>(qualifier.get());
                    parseLayoutQualifier(*layoutQualifier);
                }
                else if (qualifier->qualifier.type == sl::TokenType::Uniform)
                {
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                }
                else if (qualifier->qualifier.type == sl::TokenType::Buffer)
                {
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                }
            }
        }

        if (setId)
        {
            const size_t index = setId.value();
            if (metadata.descriptorSetLayoutBindings.size() <= index)
            {
                metadata.descriptorSetLayoutBindings.resize(index + 1, {});
            }

            if (metadata.descriptorSetLayoutBindings.at(index).size() <= binding.binding)
            {
                metadata.descriptorSetLayoutBindings.at(index).resize(binding.binding + 1, {});
            }

            metadata.descriptorSetLayoutBindings[index][binding.binding] = binding;
        }
    }

    return metadata;
}

void ShaderUniformInputMetadata::merge(ShaderUniformInputMetadata&& rhs)
{
    // Resize current, if necessary
    if (rhs.descriptorSetLayoutBindings.size() > descriptorSetLayoutBindings.size())
    {
        descriptorSetLayoutBindings.resize(rhs.descriptorSetLayoutBindings.size());
    }

    for (const auto& [setIdx, rhsLayout] : enumerate(rhs.descriptorSetLayoutBindings))
    {
        // Resize layout[i], if ncessary
        if (rhsLayout.size() > descriptorSetLayoutBindings[setIdx].size())
        {
            descriptorSetLayoutBindings[setIdx].resize(rhsLayout.size());
        }

        for (const auto& [bindingIdx, rhsBinding] : enumerate(rhsLayout))
        {
            auto& lhsLayout = descriptorSetLayoutBindings[setIdx][bindingIdx];
            if (lhsLayout.descriptorCount > 0)
            {
                lhsLayout.stageFlags |= rhsBinding.stageFlags;
            }
            // If RHS binding is valid, merge it into the current LHS
            else if (rhsBinding.descriptorCount > 0)
            {
                lhsLayout = rhsBinding;
            }
        }
    }

    pushConstants.insert(pushConstants.end(), rhs.pushConstants.begin(), rhs.pushConstants.end());
}

Result<std::vector<char>> readSpirvFile(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return resultError("Failed to open spirv file: {}!", filePath.string());
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize % sizeof(uint32_t) != 0)
    {
        return resultError(
            "File size of {} is not divisible by 4. SPIRV code is a stream of uint32_t tokens.", filePath.string());
    }

    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

namespace
{
#define SPV_TO_VK_CASE_RETURN(enumEntry)                                                                               \
    case SPV_REFLECT_##enumEntry:                                                                                      \
        return VK_##enumEntry;

#define CRISP_SPV_TRY(spvFunctionCall)                                                                                 \
    if ((spvFunctionCall) != SPV_REFLECT_RESULT_SUCCESS)                                                               \
    {                                                                                                                  \
        return resultError("Error while calling {}", #spvFunctionCall);                                                \
    }

Result<VkShaderStageFlagBits> toVulkanShaderStage(const SpvReflectShaderStageFlagBits stageBit)
{
    switch (stageBit)
    {
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

Result<VkDescriptorType> toVulkanDescriptorType(const SpvReflectDescriptorType type)
{
    switch (type)
    {
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

class SpirvShaderReflectionModuleGuard
{
public:
    explicit SpirvShaderReflectionModuleGuard(SpvReflectShaderModule& module)
        : m_module(module)
    {
    }

    ~SpirvShaderReflectionModuleGuard()
    {
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

Result<ShaderUniformInputMetadata> reflectUniformMetadataFromSpirvShader(std::span<const char> spirvShader)
{
    SpvReflectShaderModule module;
    CRISP_SPV_TRY(spvReflectCreateShaderModule(spirvShader.size(), spirvShader.data(), &module));
    SpirvShaderReflectionModuleGuard moduleGuard(module);

    const auto stageFlags = toVulkanShaderStage(module.shader_stage).unwrap();

    uint32_t totalSetCount = module.descriptor_set_count;
    for (uint32_t i = 0; i < module.descriptor_set_count; ++i)
    {
        totalSetCount = std::max(totalSetCount, module.descriptor_sets[i].set + 1);
    }

    ShaderUniformInputMetadata metadata{};
    metadata.descriptorSetLayoutBindings.resize(totalSetCount);
    for (uint32_t i = 0; i < module.descriptor_set_count; ++i)
    {
        const auto& descSet = module.descriptor_sets[i];

        uint32_t totalBindingCount = descSet.binding_count;
        for (uint32_t j = 0; j < descSet.binding_count; ++j)
        {
            totalBindingCount = std::max(totalBindingCount, descSet.bindings[j]->binding + 1); // NOLINT
        }
        metadata.descriptorSetLayoutBindings[descSet.set].resize(totalBindingCount);

        for (uint32_t j = 0; j < descSet.binding_count; ++j)
        {
            const auto& spvBinding = descSet.bindings[j]; // NOLINT
            auto& binding = metadata.descriptorSetLayoutBindings[descSet.set][spvBinding->binding];
            binding.binding = spvBinding->binding;
            binding.descriptorCount = spvBinding->count;
            binding.descriptorType = toVulkanDescriptorType(spvBinding->descriptor_type).unwrap();
            binding.stageFlags = stageFlags;
            binding.pImmutableSamplers = nullptr;
        }
    }

    metadata.pushConstants.resize(module.push_constant_block_count);
    for (uint32_t i = 0; i < module.push_constant_block_count; ++i)
    {
        metadata.pushConstants[i].offset = module.push_constant_blocks[i].offset; // NOLINT
        metadata.pushConstants[i].size = module.push_constant_blocks[i].size;     // NOLINT
        metadata.pushConstants[i].stageFlags = stageFlags;
    }

    return metadata;
}

} // namespace crisp::sl
