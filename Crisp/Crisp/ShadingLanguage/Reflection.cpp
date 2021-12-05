#include <Crisp/ShadingLanguage/Reflection.hpp>

#include <Crisp/ShadingLanguage/Lexer.hpp>
#include <Crisp/ShadingLanguage/Parser.hpp>
#include <Crisp/ShadingLanguage/ShaderType.hpp>

#include <CrispCore/IO/FileUtils.hpp>

#include <spdlog/spdlog.h>

#include <optional>
#include <fstream>

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
                        return 0;

                    if (auto bin = dynamic_cast<BinaryExpr*>(layoutQualifier->ids[0].get()))
                    {
                        auto* left = dynamic_cast<Variable*>(bin->left.get());
                        auto* right = dynamic_cast<Literal*>(bin->right.get());
                        if (left && right && left->name.lexeme == "offset")
                            return std::any_cast<int>(right->value);
                    }
                }
            }

            return 0;
        }

        uint32_t getPushConstantFieldArraySize(const StructFieldDeclaration& field)
        {
            if (field.variable->arraySpecifiers.empty())
                return 1;
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
                    case TokenType::Mat4:  fieldSize = 16 * sizeof(float); break;
                    case TokenType::Mat3:  fieldSize = 9 * sizeof(float); break;
                    case TokenType::Vec4:  fieldSize = 4 * sizeof(float); break;
                    case TokenType::Vec2:  fieldSize = 2 * sizeof(float); break;
                    case TokenType::Float: fieldSize = sizeof(float); break;
                    case TokenType::Uint:  fieldSize = sizeof(unsigned int); break;
                    case TokenType::Int:   fieldSize = sizeof(int); break;

                    default: spdlog::critical("Unknown token size '{}' while parsing push constant!", field->fullType->specifier->type.lexeme); break;
                    }
                    pc.size += fieldSize * multiplier;
                }
            }

            return pc;
        }
    }

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

            auto parseLayoutQualifier = [&metadata, &binding, &statement, &setId](const sl::LayoutQualifier& layoutQualifier)
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
                                setId = std::any_cast<int32_t>(right->value);
                            else if (left->name.lexeme == "binding")
                                binding.binding = std::any_cast<int32_t>(right->value);
                        }
                    }
                    else if (const auto identifier = dynamic_cast<sl::Variable*>(id.get()))
                    {
                        if (identifier->name.lexeme == "push_constant")
                            metadata.pushConstants.push_back(parsePushConstant(statement.get(), binding.stageFlags));
                    }
                }
            };

            if (auto initList = dynamic_cast<sl::InitDeclaratorList*>(statement.get()))
            {
                for (auto& qualifier : initList->fullType->qualifiers)
                {
                    if (qualifier->qualifier.type == sl::TokenType::Layout)
                        parseLayoutQualifier(*dynamic_cast<sl::LayoutQualifier*>(qualifier.get()));
                }

                if (!initList->fullType->specifier)
                    continue;

                auto typeLexeme = initList->fullType->specifier->type.lexeme;
                if (typeLexeme == "sampler" || typeLexeme == "samplerShadow")
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                else if (typeLexeme.find("sampler") != std::string::npos)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                else if (typeLexeme.find("texture") != std::string::npos)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                else if (typeLexeme.find("image") != std::string::npos)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                else if (typeLexeme.find("textureBuffer") != std::string::npos)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                else if (typeLexeme.find("imageBuffer") != std::string::npos)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                else if (typeLexeme.find("subpassInput") != std::string::npos)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
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
                        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    else if (qualifier->qualifier.type == sl::TokenType::Buffer)
                        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                }
            }

            if (setId)
            {
                const size_t index = setId.value();
                if (metadata.descriptorSetLayoutBindings.size() <= index)
                    metadata.descriptorSetLayoutBindings.resize(index + 1, {});

                if (metadata.descriptorSetLayoutBindings.at(index).size() <= binding.binding)
                    metadata.descriptorSetLayoutBindings.at(index).resize(binding.binding + 1, {});

                metadata.descriptorSetLayoutBindings[index][binding.binding] = binding;
            }
        }

        return metadata;
    }

    void ShaderUniformInputMetadata::merge(ShaderUniformInputMetadata&& rhs)
    {
        if (rhs.descriptorSetLayoutBindings.size() > descriptorSetLayoutBindings.size())
            descriptorSetLayoutBindings.resize(rhs.descriptorSetLayoutBindings.size());

        for (uint32_t i = 0; i < rhs.descriptorSetLayoutBindings.size(); ++i)
        {
            if (rhs.descriptorSetLayoutBindings[i].size() > descriptorSetLayoutBindings[i].size())
                descriptorSetLayoutBindings[i].resize(rhs.descriptorSetLayoutBindings[i].size());

            for (uint32_t j = 0; j < rhs.descriptorSetLayoutBindings[i].size(); ++j)
            {
                if (rhs.descriptorSetLayoutBindings[i][j].descriptorCount > 0)
                    descriptorSetLayoutBindings[i][j] = rhs.descriptorSetLayoutBindings[i][j];
            }
        }

        pushConstants.insert(pushConstants.end(), rhs.pushConstants.begin(), rhs.pushConstants.end());
    }
}
