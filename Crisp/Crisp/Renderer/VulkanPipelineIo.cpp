#include <Crisp/Renderer/VulkanPipelineIo.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/ShaderUtils/Reflection.hpp>

#include <string_view>

namespace crisp {
namespace {
const auto logger = createLoggerMt("VulkanPipelineIo");

Result<FlatHashMap<VkShaderStageFlagBits, std::string>> parseShaderFiles(const nlohmann::json& json) {
    FlatHashMap<VkShaderStageFlagBits, std::string> shaderFiles;

    const auto getPathIfExists = [&shaderFiles, &json](const std::string_view key, const VkShaderStageFlagBits stage) {
        if (json.contains(key)) {
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
    getPathIfExists("mesh", VK_SHADER_STAGE_MESH_BIT_EXT);
    getPathIfExists("task", VK_SHADER_STAGE_TASK_BIT_EXT);

    return shaderFiles;
}

bool shaderStagesMatchTessellation(const FlatHashMap<VkShaderStageFlagBits, std::string>& shaderFiles) {
    return shaderFiles.contains(VK_SHADER_STAGE_VERTEX_BIT) &&
           shaderFiles.contains(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) &&
           shaderFiles.contains(VK_SHADER_STAGE_FRAGMENT_BIT) &&
           shaderFiles.contains(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
}

[[nodiscard]] Result<> readVertexInputBindings(const nlohmann::json& arrayJson, PipelineBuilder& builder) {
    for (uint32_t i = 0; i < arrayJson.size(); ++i) {
        CRISP_CHECK(arrayJson[i].is_object());

        CRISP_CHECK(hasField<JsonType::String>(arrayJson[i], "inputRate"));
        VkVertexInputRate inputRate{};
        if (arrayJson[i]["inputRate"] == "vertex") {
            inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        } else if (arrayJson[i]["inputRate"] == "instance") {
            inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        } else {
            return resultError("Encountered unknown inputRate: {}", arrayJson[i]["inputRate"].dump());
        }

        CRISP_CHECK(hasField<JsonType::Array>(arrayJson[i], "formats"));
        std::vector<VkFormat> formats;
        for (const auto& f : arrayJson[i]["formats"]) {
            if (f == "vec3") {
                formats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
            } else if (f == "vec2") {
                formats.push_back(VK_FORMAT_R32G32_SFLOAT);
            } else if (f == "vec4") {
                formats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
            } else {
                return resultError("Encountered unknown format {}", f.dump());
            }
        }
        builder.addVertexInputBinding(i, inputRate, formats);
    }
    return {};
}

[[nodiscard]] Result<> readVertexAttributes(const nlohmann::json& json, PipelineBuilder& builder) {
    CRISP_CHECK(json.is_array());
    for (uint32_t i = 0; i < json.size(); ++i) {
        CRISP_CHECK(json[i].is_array());

        std::vector<VkFormat> formats;
        for (const auto& f : json[i]) {
            if (f == "vec3") {
                formats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
            } else if (f == "vec2") {
                formats.push_back(VK_FORMAT_R32G32_SFLOAT);
            } else if (f == "vec4") {
                formats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
            } else {
                return resultError("Encountered unknown format {}", f.dump());
            }
        }
        builder.addVertexAttributes(i, formats);
    }
    return {};
}

[[nodiscard]] Result<> readInputAssemblyState(const nlohmann::json& json, PipelineBuilder& builder) {
    CRISP_CHECK(json.is_object());

    if (json.contains("primitiveTopology")) {
        const auto& primTopology{json["primitiveTopology"]};
        if (primTopology == "lineList") {
            builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        } else if (primTopology == "pointList") {
            builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
        } else if (primTopology == "triangleList") {
            builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        } else {
            return resultError("Unknown primitive topology: {}", primTopology.dump());
        }
    }
    return {};
}

[[nodiscard]] Result<> readTessellationState(const nlohmann::json& json, PipelineBuilder& builder) {
    CRISP_CHECK(json.is_object());

    if (json.contains("controlPointCount")) {
        CRISP_CHECK(json["controlPointCount"].is_number_unsigned());
        builder.setTessellationControlPoints(json["controlPointCount"].get<uint32_t>());
    }
    return {};
}

[[nodiscard]] Result<> readViewportState(
    const nlohmann::json& json, const VulkanRenderPass& renderPass, PipelineBuilder& builder) {
    if (json.contains("viewports")) {
        CRISP_CHECK(json["viewports"].is_array());
        for (const auto& viewport : json["viewports"]) {
            if (viewport == "pass") {
                builder.setViewport(renderPass.createViewport());
            }
        }
        CRISP_CHECK(json["scissors"].is_array());
        for (const auto& scissor : json["scissors"]) {
            if (scissor == "pass") {
                builder.setScissor(renderPass.createScissor());
            }
        }
    }
    return {};
}

[[nodiscard]] Result<> readRasterizationState(const nlohmann::json& json, PipelineBuilder& builder) {
    CRISP_CHECK(json.is_object());

    if (json.contains("cullMode")) {
        CRISP_CHECK(json["cullMode"].is_string());
        if (json["cullMode"] == "front") {
            builder.setCullMode(VK_CULL_MODE_FRONT_BIT);
        } else if (json["cullMode"] == "back") {
            builder.setCullMode(VK_CULL_MODE_BACK_BIT);
        } else if (json["cullMode"] == "none") {
            builder.setCullMode(VK_CULL_MODE_NONE);
        } else {
            return resultError("Encountered unknown cull mode {}", json["cullMode"].dump());
        }
    }
    if (json.contains("polygonMode")) {
        CRISP_CHECK(json["polygonMode"].is_string());
        if (json["polygonMode"] == "line") {
            builder.setPolygonMode(VK_POLYGON_MODE_LINE);
        } else if (json["polygonMode"] == "fill") {
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
        } else {
            return resultError("Encountered unknown polygon mode {}", json["polygonMode"].dump());
        }
    }
    if (json.contains("lineWidth")) {
        CRISP_CHECK(json["lineWidth"].is_number());
        builder.setLineWidth(json["lineWidth"].get<float>());
    }
    return {};
}

[[nodiscard]] Result<> readMultisampleState(const nlohmann::json& json, PipelineBuilder& builder) {
    CRISP_CHECK(json.is_object());

    if (json.contains("alphaToCoverage")) {
        CRISP_CHECK(json["alphaToCoverage"].is_boolean());
        builder.setAlphaToCoverage(json["alphaToCoverage"].get<bool>());
    }
    return {};
}

[[nodiscard]] Result<> readBlendState(const nlohmann::json& json, PipelineBuilder& builder) {
    CRISP_CHECK(json.is_object());

    auto parseBlendFactor = [](const nlohmann::json& json) {
        if (json == "one") {
            return VK_BLEND_FACTOR_ONE;
        }
        if (json == "oneMinusSrcAlpha") {
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        }
        if (json == "zero") {
            return VK_BLEND_FACTOR_ZERO;
        }
        return VK_BLEND_FACTOR_MAX_ENUM;
    };

    if (json.contains("enabled")) {
        CRISP_CHECK(json["enabled"].is_boolean());
        builder.setBlendState(0, json["enabled"].get<bool>());
        builder.setBlendFactors(
            0,
            json.contains("src") ? parseBlendFactor(json["src"]) : VK_BLEND_FACTOR_ONE,
            json.contains("dst") ? parseBlendFactor(json["dst"]) : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    }

    return {};
}

[[nodiscard]] Result<> readDepthStencilState(const nlohmann::json& json, PipelineBuilder& builder) {
    CRISP_CHECK(json.is_object());

    if (json.contains("reverseDepth")) {
        CRISP_CHECK(json["reverseDepth"].is_boolean());
        if (json["reverseDepth"].get<bool>()) {
            builder.setDepthTestOperation(VK_COMPARE_OP_GREATER_OR_EQUAL);
        }
    }

    if (json.contains("depthWriteEnabled")) {
        CRISP_CHECK(json["depthWriteEnabled"].is_boolean());
        builder.setDepthWrite(json["depthWriteEnabled"].get<bool>());
    }

    if (json.contains("depthTest")) {
        CRISP_CHECK(json["depthTest"].is_boolean());
        if (json["depthTest"].get<bool>()) {
            builder.setDepthTest(json["depthTest"].get<bool>());
        }
    }

    return {};
}

[[nodiscard]] Result<> readDescriptorSetMetadata(const nlohmann::json& json, PipelineLayoutBuilder& layoutBuilder) {
    for (int32_t i = 0; i < static_cast<int32_t>(json.size()); ++i) {
        CRISP_CHECK(json[i].is_object());
        CRISP_CHECK(hasField<JsonType::Boolean>(json[i], "buffered"));
        layoutBuilder.setDescriptorSetBuffering(i, json[i]["buffered"].get<bool>());

        if (hasField<JsonType::Array>(json[i], "bindless")) {
            CRISP_CHECK_EQ(json[i]["bindless"].size(), 2);
            const auto& arr = json[i]["bindless"];
            layoutBuilder.setDescriptorBindless(i, arr[0].get<uint32_t>(), arr[1].get<uint32_t>());
        }

        if (hasField<JsonType::Array>(json[i], "dynamicBuffers")) {
            for (int32_t j = 0; j < static_cast<int32_t>(json[i]["dynamicBuffers"].size()); ++j) {
                CRISP_CHECK(json[i]["dynamicBuffers"][j].is_number_unsigned());
                layoutBuilder.setDescriptorDynamic(i, json[i]["dynamicBuffers"][j].get<int32_t>(), true);
            }
        }
    }
    return {};
}

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromJson(
    const nlohmann::json& pipelineJson,
    const std::filesystem::path& spvShaderDir,
    ShaderCache& shaderCache,
    const VulkanDevice& device,
    const VulkanRenderPass& renderPass,
    const uint32_t subpassIndex) {
    CRISP_CHECK(pipelineJson.is_object());

    CRISP_CHECK(hasField<JsonType::Object>(pipelineJson, "shaders"));
    const auto shaderFiles{parseShaderFiles(pipelineJson["shaders"]).unwrap()};

    PipelineLayoutMetadata shaderMetadata{};
    PipelineBuilder builder{};
    for (const auto& [stageFlag, fileStem] : shaderFiles) {
        const auto absoluteSpvPath = spvShaderDir / (fileStem + ".spv");
        builder.addShaderStage(createShaderStageInfo(stageFlag, shaderCache.getOrLoadShaderModule(absoluteSpvPath)));
        const auto spvFile = readSpirvFile(absoluteSpvPath).unwrap();
        shaderMetadata.merge(reflectPipelineLayoutFromSpirvShader(spvFile).unwrap());
    }

    if (shaderStagesMatchTessellation(shaderFiles)) {
        // Assume that we are dealing with quad tessellation for now.
        builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
    }

    CRISP_CHECK(hasField<JsonType::Array>(pipelineJson, "vertexInputBindings"));
    readVertexInputBindings(pipelineJson["vertexInputBindings"], builder).unwrap();

    CRISP_CHECK(hasField<JsonType::Array>(pipelineJson, "vertexAttributes"));
    readVertexAttributes(pipelineJson["vertexAttributes"], builder).unwrap();

    // Optional state for overrides.
    if (hasField<JsonType::Object>(pipelineJson, "inputAssembly")) {
        readInputAssemblyState(pipelineJson["inputAssembly"], builder).unwrap();
    }

    // Optional state for tessellation only.
    if (hasField<JsonType::Object>(pipelineJson, "tessellation")) {
        readTessellationState(pipelineJson["tessellation"], builder).unwrap();
    }

    // Optional state - if no viewport information is supposed, we assume screen size.
    if (hasField<JsonType::Object>(pipelineJson, "viewport")) {
        readViewportState(pipelineJson["viewport"], renderPass, builder).unwrap();
    } else {
        builder.setViewport({}).addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
        builder.setScissor({}).addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    }

    // Optional state for overrides.
    if (hasField<JsonType::Object>(pipelineJson, "rasterization")) {
        readRasterizationState(pipelineJson["rasterization"], builder).unwrap();
    }

    if (hasField<JsonType::Object>(pipelineJson, "multisample")) {
        readMultisampleState(pipelineJson["multisample"], builder).unwrap();
    }

    if (hasField<JsonType::Object>(pipelineJson, "blend")) {
        readBlendState(pipelineJson["blend"], builder).unwrap();
    }

    if (hasField<JsonType::Object>(pipelineJson, "depthStencil")) {
        readDepthStencilState(pipelineJson["depthStencil"], builder).unwrap();
    }

    PipelineLayoutBuilder layoutBuilder(std::move(shaderMetadata));
    if (hasField<JsonType::Array>(pipelineJson, "descriptorSets")) {
        readDescriptorSetMetadata(pipelineJson["descriptorSets"], layoutBuilder).unwrap();
    }

    return builder.create(device, layoutBuilder.create(device), renderPass.getHandle(), subpassIndex);
}

} // namespace

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromFile(
    const std::filesystem::path& path,
    const std::filesystem::path& spvShaderDir,
    ShaderCache& shaderCache,
    const VulkanDevice& device,
    const VulkanRenderPass& renderPass,
    const uint32_t subpassIndex) {
    auto maybeJson = loadJsonFromFile(path);
    if (!maybeJson) {
        return resultError("Failed to open json config at {}", path.generic_string());
    }

    auto result =
        createPipelineFromJson(maybeJson.unwrap(), spvShaderDir, shaderCache, device, renderPass, subpassIndex);
    if (result) {
        device.setObjectName((*result)->getHandle(), fmt::format("{} Pipeline", path.stem().string()));
    }
    return result;
}

} // namespace crisp
