#include "Reflection.hpp"

#include <optional>
#include <fstream>
#include <IO/FileUtils.hpp>

#include "Lexer.hpp"
#include "Parser.hpp"

namespace
{
    VkPushConstantRange parsePushConstant(const crisp::sl::Statement* statement, VkShaderStageFlags stage)
    {
        using namespace crisp::sl;
        VkPushConstantRange pc = {};
        pc.stageFlags = stage;

        const auto* block = dynamic_cast<const BlockDeclaration*>(statement);
        if (block)
        {
            for (const auto& field : block->fields)
            {
                auto tokenType = field->fullType->specifier->type.type;
                switch (tokenType)
                {
                case TokenType::Mat4: pc.size += 16 * sizeof(float); break;
                case TokenType::Mat3: pc.size += 9 * sizeof(float); break;
                }
            }
        }

        return pc;
    }
}

namespace crisp::sl
{
    Reflection::Reflection()
    {
    }

    void Reflection::parseDescriptorSets(const std::filesystem::path& sourcePath)
    {
        auto shaderType = sourcePath.stem().string();
        auto shaderTypeOffset = shaderType.find_last_of('-');
        if (shaderType.size() > 4)
            shaderType = shaderType.substr(shaderTypeOffset + 1);

        VkShaderStageFlags stageFlags = 0;
        if (shaderType == "vert")
            stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        else if (shaderType == "frag")
            stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        else if (shaderType == "tesc")
            stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        else if (shaderType == "tese")
            stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        else if (shaderType == "geom")
            stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
        else if (shaderType == "comp")
            stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        auto tokens = sl::Lexer(sourcePath).scanTokens();
        auto statements = sl::Parser(tokens).parse();

        for (const auto& statement : statements)
        {
            std::optional<uint32_t> setId;
            VkDescriptorSetLayoutBinding binding = {};
            binding.descriptorCount = 1;
            binding.stageFlags = stageFlags;
            bool isDynamic = false;
            bool isBuffered = false;

            auto parseLayoutQualifier = [&](const sl::LayoutQualifier& layoutQualifier)
            {
                for (auto& id : layoutQualifier.ids)
                {
                    if (auto bin = dynamic_cast<sl::BinaryExpr*>(id.get()))
                    {
                        auto* left = dynamic_cast<sl::Variable*>(bin->left.get());
                        auto* right = dynamic_cast<sl::Literal*>(bin->right.get());
                        if (left && right)
                        {
                            if (left->name.lexeme == "set")
                                setId = std::any_cast<int>(right->value);
                            else if (left->name.lexeme == "binding")
                                binding.binding = std::any_cast<int>(right->value);
                        }
                    }
                    else if (auto identifier = dynamic_cast<sl::Variable*>(id.get()))
                    {
                        if (identifier->name.lexeme == "dynamic")
                            isDynamic = true;
                        else if (identifier->name.lexeme == "buffered")
                            isBuffered = true;
                        else if (identifier->name.lexeme == "push_constant")
                            m_pushConstants.push_back(parsePushConstant(statement.get(), stageFlags));
                    }
                }
            };

            if (auto initList = dynamic_cast<sl::InitDeclaratorList*>(statement.get()))
            {
                for (auto& qualifier : initList->fullType->qualifiers)
                {
                    if (qualifier->qualifier.type == sl::TokenType::Layout)
                    {
                        auto layoutQualifier = dynamic_cast<sl::LayoutQualifier*>(qualifier.get());
                        parseLayoutQualifier(*layoutQualifier);
                    }
                }

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
                for (auto& qualifier : block->qualifiers)
                {
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

                if (isDynamic)
                    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                    else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            }

            if (setId)
                addSetLayoutBinding(setId.value(), binding, isBuffered);
        }
    }

    uint32_t Reflection::getDescriptorSetCount() const
    {
        return static_cast<uint32_t>(m_setLayoutBindings.size());
    }

    std::vector<VkDescriptorSetLayoutBinding> Reflection::getDescriptorSetLayouts(uint32_t index) const
    {
        return m_setLayoutBindings.at(index);
    }

    bool Reflection::isSetBuffered(uint32_t index) const
    {
        return m_isSetBuffered.at(index);
    }

    std::vector<VkPushConstantRange> Reflection::getPushConstants() const
    {
        return m_pushConstants;
    }

    void Reflection::addSetLayoutBinding(uint32_t setId, const VkDescriptorSetLayoutBinding& binding, bool isBuffered)
    {
        if (m_setLayoutBindings.size() <= setId)
            m_setLayoutBindings.resize(setId + 1);

        if (m_setLayoutBindings[setId].size() <= binding.binding)
            m_setLayoutBindings[setId].resize(binding.binding + 1);

        auto prevFlags = m_setLayoutBindings[setId][binding.binding].stageFlags;
        m_setLayoutBindings[setId][binding.binding] = binding;
        m_setLayoutBindings[setId][binding.binding].stageFlags |= prevFlags;

        if (m_isSetBuffered.size() <= setId)
            m_isSetBuffered.resize(setId + 1);

        m_isSetBuffered[setId] = m_isSetBuffered[setId] | isBuffered;
    }
}
