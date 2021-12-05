#include "ShaderType.hpp"

namespace crisp
{
    namespace
    {
        const std::filesystem::path GlslExtension = ".glsl";
        const robin_hood::unordered_flat_map<std::string, VkShaderStageFlagBits> ShaderStageMap =
        {
            {"vert", VK_SHADER_STAGE_VERTEX_BIT},
            {"frag", VK_SHADER_STAGE_FRAGMENT_BIT},
            {"tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT},
            {"tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
            {"geom", VK_SHADER_STAGE_GEOMETRY_BIT},
            {"comp", VK_SHADER_STAGE_COMPUTE_BIT},
            {"rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR},
            {"rchit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
            {"rmiss", VK_SHADER_STAGE_MISS_BIT_KHR},
        };
    }

    Result<VkShaderStageFlags> getShaderStageFromFilePath(const std::filesystem::path& glslShaderFilePath)
    {
        // Convert "../path/to/shader.type.glsl" to type
        return getShaderStageFromShaderType(glslShaderFilePath.stem().extension().string().substr(1));
    }

    Result<VkShaderStageFlags> getShaderStageFromShaderType(const std::string& shaderType)
    {
        if (shaderType.empty())
            return resultError("Empty string passed to getShaderStageFromShaderType()!");

        if (const auto it = ShaderStageMap.find(shaderType); it != ShaderStageMap.end())
            return it->second;

        return resultError("Unknown shader stage: {}!", shaderType.size());
    }
}


