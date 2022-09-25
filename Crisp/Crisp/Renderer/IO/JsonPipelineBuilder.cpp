#include <Crisp/Renderer/IO/JsonPipelineBuilder.hpp>

#include <Crisp/Common/Checks.hpp>
#include <Crisp/Common/Logger.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/ShadingLanguage/Reflection.hpp>

#include <string_view>

namespace crisp
{
namespace
{
auto logger = createLoggerMt("JsonPipelineBuilder");

enum class JsonType
{
    Null,
    Object,
    Array,
    String,
    Boolean,
    NumberInt,
    NumberUint,
    NumberFloat,
    Binary
};

template <JsonType type>
constexpr nlohmann::detail::value_t toNlohmannJsonType()
{
    if constexpr (type == JsonType::Null)
        return nlohmann::detail::value_t::null;
    if constexpr (type == JsonType::Object)
        return nlohmann::detail::value_t::object;
    if constexpr (type == JsonType::Array)
        return nlohmann::detail::value_t::array;
    if constexpr (type == JsonType::String)
        return nlohmann::detail::value_t::string;
    if constexpr (type == JsonType::Boolean)
        return nlohmann::detail::value_t::boolean;
    if constexpr (type == JsonType::NumberInt)
        return nlohmann::detail::value_t::number_integer;
    if constexpr (type == JsonType::NumberUint)
        return nlohmann::detail::value_t::number_unsigned;
    if constexpr (type == JsonType::NumberFloat)
        return nlohmann::detail::value_t::number_float;
    if constexpr (type == JsonType::Binary)
        return nlohmann::detail::value_t::binary;
}

template <JsonType fieldType>
bool hasField(const nlohmann::json& json, const std::string_view key)
{
    return json.contains(key) && json[key].type() == toNlohmannJsonType<fieldType>();
}

Result<HashMap<VkShaderStageFlagBits, std::string>> readShaderFiles(const nlohmann::json& json)
{
    HashMap<VkShaderStageFlagBits, std::string> shaderFiles;

    const auto getPathIfExists = [&shaderFiles, &json](const std::string_view key, const VkShaderStageFlagBits stage)
    {
        if (json.contains(key))
        {
            CRISP_CHECK(json[key].is_string());
            shaderFiles.emplace(stage, json[key].get<std::string>());
        }
    };
    getPathIfExists("vert", VK_SHADER_STAGE_VERTEX_BIT);
    getPathIfExists("frag", VK_SHADER_STAGE_FRAGMENT_BIT);
    getPathIfExists("geom", VK_SHADER_STAGE_GEOMETRY_BIT);
    getPathIfExists("tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    getPathIfExists("tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    getPathIfExists("comp", VK_SHADER_STAGE_COMPUTE_BIT);

    return shaderFiles;
}

[[nodiscard]] Result<> readVertexInputBindings(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_array());
    for (uint32_t i = 0; i < json.size(); ++i)
    {
        CRISP_CHECK(json[i].is_object());

        CRISP_CHECK(hasField<JsonType::String>(json[i], "inputRate"));
        const auto inputRate{
            json[i]["inputRate"] == "vertex" ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE};

        CRISP_CHECK(hasField<JsonType::Array>(json[i], "formats"));
        std::vector<VkFormat> formats;
        for (const auto& f : json[i]["formats"])
        {
            if (f == "vec3")
                formats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
            else if (f == "vec2")
                formats.push_back(VK_FORMAT_R32G32_SFLOAT);
            else if (f == "vec4")
                formats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
            else
                return resultError("Encountered unknown format {}", f);
        }
        builder.addVertexInputBinding(i, inputRate, formats);
    }
    return {};
}

[[nodiscard]] Result<> readVertexAttributes(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_array());
    for (uint32_t i = 0; i < json.size(); ++i)
    {
        CRISP_CHECK(json[i].is_array());

        std::vector<VkFormat> formats;
        for (const auto& f : json[i])
        {
            if (f == "vec3")
                formats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
            else if (f == "vec2")
                formats.push_back(VK_FORMAT_R32G32_SFLOAT);
            else if (f == "vec4")
                formats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
            else
                return resultError("Encountered unknown format {}", f);
        }
        builder.addVertexAttributes(i, formats);
    }
    return {};
}

[[nodiscard]] Result<> readInputAssemblyState(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_object());

    if (json.contains("primitiveTopology"))
    {
        const auto& primTopology{json["primitiveTopology"]};
        if (primTopology == "lineList")
        {
            builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        }
        else if (primTopology == "pointList")
        {
            builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
        }
    }
    return {};
}

[[nodiscard]] Result<> readTessellationState(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_object());

    if (json.contains("controlPointCount"))
    {
        CRISP_CHECK(json["controlPointCount"].is_number_unsigned());
        builder.setTessellationControlPoints(json["controlPointCount"].get<uint32_t>());
    }
    return {};
}

[[nodiscard]] Result<> readViewportState(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_object());

    if (json.contains("controlPointCount"))
    {
        CRISP_CHECK(json["controlPointCount"].is_number_unsigned());
        builder.setTessellationControlPoints(json["controlPointCount"].get<uint32_t>());
    }
    return {};
}

[[nodiscard]] Result<> readRasterizationState(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_object());

    if (json.contains("cullMode"))
    {
        CRISP_CHECK(json["cullMode"].is_string());
        if (json["cullMode"] == "front")
        {
            builder.setCullMode(VK_CULL_MODE_FRONT_BIT);
        }
        else if (json["cullMode"] == "none")
        {
            builder.setCullMode(VK_CULL_MODE_NONE);
        }
        else
            return resultError("Encountered unknown cull mode {}", json["cullMode"]);
    }
    if (json.contains("polygonMode"))
    {
        CRISP_CHECK(json["polygonMode"].is_string());
        if (json["polygonMode"] == "line")
        {
            builder.setCullMode(VK_POLYGON_MODE_LINE);
        }
        else if (json["polygonMode"] == "fill")
        {
            builder.setCullMode(VK_POLYGON_MODE_FILL);
        }
        else
            return resultError("Encountered unknown polygon mode {}", json["polygonMode"]);
    }
    if (json.contains("lineWidth"))
    {
        CRISP_CHECK(json["lineWidth"].is_number());
        builder.setLineWidth(json["lineWidth"].get<float>());
    }
    return {};
}

[[nodiscard]] Result<> readMultisampleState(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_object());

    if (json.contains("alphaToCoverage"))
    {
        CRISP_CHECK(json["alphaToCoverage"].is_boolean());
        builder.setAlphaToCoverage(json["alphaToCoverage"].get<bool>());
    }
    return {};
}

[[nodiscard]] Result<> readBlendState(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_object());

    auto parseBlendFactor = [](const nlohmann::json& json, const VkBlendFactor defaultValue)
    {
        if (json.is_null())
            return defaultValue;

        if (json == "one")
            return VK_BLEND_FACTOR_ONE;
        else if (json == "oneMinusSrcAlpha")
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        else if (json == "zero")
            return VK_BLEND_FACTOR_ZERO;
        else
            return VK_BLEND_FACTOR_MAX_ENUM;
    };

    if (json.contains("enabled"))
    {
        CRISP_CHECK(json["enabled"].is_boolean());
        builder.setBlendState(0, json["enabled"].get<bool>());
        builder.setBlendFactors(
            0,
            parseBlendFactor(json["src"], VK_BLEND_FACTOR_ONE),
            parseBlendFactor(json["dst"], VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA));
    }

    return {};
}

[[nodiscard]] Result<> readDepthStencilState(const nlohmann::json& json, PipelineBuilder& builder)
{
    CRISP_CHECK(json.is_object());

    if (json.contains("reverseDepth"))
    {
        CRISP_CHECK(json["reverseDepth"].is_boolean());
        if (json["reverseDepth"].get<bool>())
        {
            builder.setDepthTestOperation(VK_COMPARE_OP_GREATER_OR_EQUAL);
        }
    }

    if (json.contains("depthWriteEnabled"))
    {
        CRISP_CHECK(json["depthWriteEnabled"].is_boolean());
        builder.setDepthWrite(json["depthWriteEnabled"].get<bool>());
    }

    if (json.contains("depthTest"))
    {
        CRISP_CHECK(json["depthTest"].is_boolean());
        if (json["depthTest"].get<bool>())
        {
            builder.setDepthTest(json["depthTest"].get<bool>());
        }
    }

    return {};
}

[[nodiscard]] Result<> readDescriptorSetMetadata(const nlohmann::json& json, PipelineLayoutBuilder& layoutBuilder)
{
    for (uint32_t i = 0; i < json.size(); ++i)
    {
        CRISP_CHECK(json[i].is_object());
        CRISP_CHECK(hasField<JsonType::Boolean>(json[i], "buffered"));
        layoutBuilder.setDescriptorSetBuffering(i, json[i]["buffered"].get<bool>());

        if (hasField<JsonType::Array>(json[i], "dynamicBuffers"))
        {
            for (uint32_t j = 0; j < json[i]["dynamicBuffers"].size(); ++j)
            {
                CRISP_CHECK(json[i]["dynamicBuffers"][j].is_number_unsigned());
                layoutBuilder.setDescriptorDynamic(i, json[i]["dynamicBuffers"][j].get<uint32_t>(), true);
            }
        }
    }
    return {};
}

} // namespace

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromJson(
    const nlohmann::json& pipelineJson,
    Renderer& renderer,
    const VulkanRenderPass& renderPass,
    const uint32_t subpassIndex)
{
    CRISP_CHECK(pipelineJson.is_object());
    PipelineBuilder builder{};

    CRISP_CHECK(hasField<JsonType::Object>(pipelineJson, "shaders"));
    const auto shaderFiles{readShaderFiles(pipelineJson["shaders"]).unwrap()};

    sl::ShaderUniformInputMetadata shaderMetadata{};
    for (const auto& [stageFlag, fileStem] : shaderFiles)
    {
        renderer.loadShaderModule(fileStem);
        shaderMetadata.merge(sl::parseShaderUniformInputMetadata(renderer.getShaderSourcePath(fileStem)).unwrap());
        builder.addShaderStage(createShaderStageInfo(stageFlag, renderer.getShaderModule(fileStem)));
    }

    if (shaderFiles.size() == 4)
    {
        // Assume that we are dealing with tessellation for now.
        builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
    }

    CRISP_CHECK(hasField<JsonType::Array>(pipelineJson, "vertexInputBindings"));
    readVertexInputBindings(pipelineJson["vertexInputBindings"], builder).unwrap();

    CRISP_CHECK(hasField<JsonType::Array>(pipelineJson, "vertexAttributes"));
    readVertexAttributes(pipelineJson["vertexAttributes"], builder).unwrap();

    // Optional state for overrides.
    if (hasField<JsonType::Object>(pipelineJson, "inputAssembly"))
    {
        readInputAssemblyState(pipelineJson["inputAssembly"], builder).unwrap();
    }

    // Optional state for tessellation only.
    if (hasField<JsonType::Object>(pipelineJson, "tessellation"))
    {
        readTessellationState(pipelineJson["tessellation"], builder).unwrap();
    }

    // Optional state - if no viewport information is supposed, we assume screen size.
    if (hasField<JsonType::Object>(pipelineJson, "viewport"))
    {
        readViewportState(pipelineJson["viewport"], builder).unwrap();
    }
    else
    {
        builder.setViewport(renderer.getDefaultViewport()).addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
        builder.setScissor(renderer.getDefaultScissor()).addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    }

    // Optional state for overrides.
    if (hasField<JsonType::Object>(pipelineJson, "rasterization"))
    {
        readRasterizationState(pipelineJson["rasterization"], builder).unwrap();
    }

    if (hasField<JsonType::Object>(pipelineJson, "multisample"))
    {
        readMultisampleState(pipelineJson["multisample"], builder).unwrap();
    }

    if (hasField<JsonType::Object>(pipelineJson, "blend"))
    {
        readBlendState(pipelineJson["blend"], builder).unwrap();
    }

    if (hasField<JsonType::Object>(pipelineJson, "depthStencil"))
    {
        readDepthStencilState(pipelineJson["depthStencil"], builder).unwrap();
    }

    PipelineLayoutBuilder layoutBuilder(std::move(shaderMetadata));
    CRISP_CHECK(hasField<JsonType::Array>(pipelineJson, "descriptorSets"));
    readDescriptorSetMetadata(pipelineJson["descriptorSets"], layoutBuilder).unwrap();

    auto layout = layoutBuilder.create(renderer.getDevice());

    auto pipeline = builder.create(renderer.getDevice(), std::move(layout), renderPass.getHandle(), subpassIndex);
    // pipeline->setTag(m_configName);
    return pipeline;
}

} // namespace crisp
