#include <Crisp/Vulkan/VulkanPipelineIo.hpp>

#include <bit>
#include <span>
#include <string_view>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/ShaderUtils/Reflection.hpp>
#include <Crisp/Vulkan/PipelineBuilder.hpp>
#include <Crisp/Vulkan/PipelineLayoutBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {
// Specialization data is untyped bytes, so every alternative collapses to the same four.
uint32_t toSpecializationBytes(const SpecializationConstant& value) {
    return std::visit([](const auto alternative) { return std::bit_cast<uint32_t>(alternative); }, value);
}

const auto logger = createLoggerMt("VulkanPipelineIo");

class ScopedShaderModules {
public:
    explicit ScopedShaderModules(const VulkanDevice& device)
        : m_device(device) {}

    ~ScopedShaderModules() {
        for (const auto module : m_modules) {
            vkDestroyShaderModule(m_device.getHandle(), module, nullptr);
        }
    }

    VkShaderModule create(const std::span<const char> code, const std::string& debugName) {
        const VkShaderModuleCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code.size_bytes(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()), // NOLINT
        };
        VkShaderModule module{VK_NULL_HANDLE};
        VK_FATAL(vkCreateShaderModule(m_device.getHandle(), &createInfo, nullptr, &module));
        m_device.setObjectName(module, debugName);
        m_modules.push_back(module);
        return module;
    }

private:
    const VulkanDevice& m_device;
    std::vector<VkShaderModule> m_modules;
};

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

[[nodiscard]] Result<> readVertexInputBindings(
    const nlohmann::json& arrayJson, const ShaderVertexInputMetadata& reflectedMetadata, PipelineBuilder& builder) {
    std::vector<bool> assignedAttributes(reflectedMetadata.attributes.size(), false);

    for (uint32_t i = 0; i < arrayJson.size(); ++i) {
        const auto& bindingJson = arrayJson[i];
        if (!bindingJson.is_object()) {
            return resultError("Vertex input binding {} must be an object.", i);
        }
        for (const auto& [key, _] : bindingJson.items()) {
            if (key != "locations" && key != "inputRate") {
                return resultError("Vertex input binding {} contains unknown field '{}'.", i, key);
            }
        }

        if (!hasField<JsonType::String>(bindingJson, "inputRate")) {
            return resultError("Vertex input binding {} field 'inputRate' must be a string.", i);
        }
        VkVertexInputRate inputRate{};
        if (bindingJson["inputRate"] == "vertex") {
            inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        } else if (bindingJson["inputRate"] == "instance") {
            inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        } else {
            return resultError("Vertex input binding {} has unknown inputRate {}.", i, bindingJson["inputRate"].dump());
        }

        if (!hasField<JsonType::Array>(bindingJson, "locations")) {
            return resultError("Vertex input binding {} field 'locations' must be an array.", i);
        }

        std::vector<uint32_t> locations;
        std::vector<VkFormat> formats;
        locations.reserve(bindingJson["locations"].size());
        formats.reserve(bindingJson["locations"].size());
        for (const auto& locationJson : bindingJson["locations"]) {
            if (!locationJson.is_number_unsigned()) {
                return resultError("Vertex input binding {} locations must be unsigned integers.", i);
            }

            const uint32_t location = locationJson.get<uint32_t>();
            std::size_t reflectedIndex = 0;
            while (reflectedIndex < reflectedMetadata.attributes.size() &&
                   reflectedMetadata.attributes[reflectedIndex].location != location) {
                ++reflectedIndex;
            }
            if (reflectedIndex == reflectedMetadata.attributes.size()) {
                return resultError(
                    "Vertex input binding {} references location {}, which the shader does not use.", i, location);
            }
            if (assignedAttributes[reflectedIndex]) {
                return resultError("Shader vertex input location {} is assigned to more than one binding.", location);
            }

            assignedAttributes[reflectedIndex] = true;
            locations.push_back(location);
            formats.push_back(reflectedMetadata.attributes[reflectedIndex].format);
        }

        builder.addVertexInputBinding(i, inputRate, formats);
        builder.addVertexAttributes(i, locations, formats);
    }

    for (std::size_t i = 0; i < assignedAttributes.size(); ++i) {
        if (!assignedAttributes[i]) {
            const auto& attribute = reflectedMetadata.attributes[i];
            return resultError(
                "Shader vertex input '{}' at location {} is not assigned to a vertex buffer binding.",
                attribute.name,
                attribute.location);
        }
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

[[nodiscard]] Result<> readViewportState(const nlohmann::json& json, PipelineBuilder& builder) {
    if (json.contains("viewports")) {
        CRISP_CHECK(json["viewports"].is_array());
        for (const auto& viewport : json["viewports"]) {
            if (viewport == "pass") {
                builder.setViewport({}).addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
            }
        }
        CRISP_CHECK(json["scissors"].is_array());
        for (const auto& scissor : json["scissors"]) {
            if (scissor == "pass") {
                builder.setScissor({}).addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
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
    if (json.contains("depthBias")) {
        const auto& depthBias = json["depthBias"];
        CRISP_CHECK(depthBias.is_object());
        CRISP_CHECK(depthBias.contains("constantFactor") && depthBias["constantFactor"].is_number());
        CRISP_CHECK(depthBias.contains("slopeFactor") && depthBias["slopeFactor"].is_number());
        CRISP_CHECK(!depthBias.contains("clamp") || depthBias["clamp"].is_number());
        builder.setDepthBias(
            depthBias["constantFactor"].get<float>(),
            depthBias["slopeFactor"].get<float>(),
            depthBias.value("clamp", 0.0f));
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

[[nodiscard]] Result<> readDescriptorSetMetadata(
    const nlohmann::json& json,
    PipelineLayoutBuilder& layoutBuilder,
    const VkDescriptorSetLayout bindlessDescriptorSetLayout) {
    for (int32_t i = 0; i < static_cast<int32_t>(json.size()); ++i) {
        const auto& setJson = json[i];
        if (!setJson.is_object()) {
            return resultError("Descriptor set {} metadata must be an object.", i);
        }
        if (static_cast<std::size_t>(i) >= layoutBuilder.getDescriptorSetLayoutCount()) {
            return resultError(
                "Descriptor set {} is configured, but shader reflection found only {} set(s).",
                i,
                layoutBuilder.getDescriptorSetLayoutCount());
        }

        for (const auto& [key, _] : setJson.items()) {
            if (key != "buffered" && key != "layout" && key != "bindless" && key != "dynamicBuffers") {
                return resultError("Descriptor set {} contains unknown field '{}'.", i, key);
            }
        }

        if (setJson.contains("layout")) {
            if (!setJson["layout"].is_string()) {
                return resultError("Descriptor set {} field 'layout' must be a string.", i);
            }
            if (setJson.size() != 1) {
                return resultError("Descriptor set {} with a shared layout cannot declare other metadata.", i);
            }
            const auto& layoutName = setJson["layout"].get_ref<const std::string&>();
            if (layoutName != "bindless") {
                return resultError("Descriptor set {} references unknown shared layout '{}'.", i, layoutName);
            }
            if (bindlessDescriptorSetLayout == VK_NULL_HANDLE) {
                return resultError("Descriptor set {} is bindless, but no bindless layout was provided.", i);
            }
            layoutBuilder.useExternalDescriptorSet(i, bindlessDescriptorSetLayout);
            continue;
        }

        if (!hasField<JsonType::Boolean>(setJson, "buffered")) {
            return resultError("Descriptor set {} field 'buffered' must be a boolean.", i);
        }
        layoutBuilder.setDescriptorSetBuffering(i, setJson["buffered"].get<bool>());

        if (setJson.contains("bindless")) {
            if (!setJson["bindless"].is_array() || setJson["bindless"].size() != 2 ||
                !setJson["bindless"][0].is_number_unsigned() || !setJson["bindless"][1].is_number_unsigned()) {
                return resultError(
                    "Descriptor set {} field 'bindless' must be [binding, descriptorCount] using unsigned integers.", i);
            }
            const auto& arr = setJson["bindless"];
            if (arr[1].get<uint32_t>() == 0) {
                return resultError("Descriptor set {} bindless descriptor count must be greater than zero.", i);
            }
            layoutBuilder.setDescriptorBindless(i, arr[0].get<uint32_t>(), arr[1].get<uint32_t>());
        }

        if (setJson.contains("dynamicBuffers")) {
            if (!setJson["dynamicBuffers"].is_array()) {
                return resultError("Descriptor set {} field 'dynamicBuffers' must be an array.", i);
            }
            for (const auto& binding : setJson["dynamicBuffers"]) {
                if (!binding.is_number_unsigned()) {
                    return resultError(
                        "Descriptor set {} field 'dynamicBuffers' must contain only unsigned integers.", i);
                }
                layoutBuilder.setDescriptorDynamic(i, binding.get<uint32_t>(), true);
            }
        }
    }
    return {};
}

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromJson(
    const nlohmann::json& pipelineJson,
    const std::filesystem::path& spvShaderDir,
    const VulkanDevice& device,
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor,
    const VkDescriptorSetLayout bindlessDescriptorSetLayout,
    const SpecializationConstantMap& specializationConstants) {
    CRISP_CHECK(pipelineJson.is_object());

    CRISP_CHECK(hasField<JsonType::Object>(pipelineJson, "shaders"));
    const auto shaderFiles{parseShaderFiles(pipelineJson["shaders"]).unwrap()};

    PipelineLayoutMetadata shaderMetadata{};
    ShaderVertexInputMetadata vertexInputMetadata{};
    PipelineBuilder builder{};
    ScopedShaderModules shaderModules(device);

    // Every stage points at these, so they must outlive builder.create() at the end of this function. An entry
    // whose constant_id a stage does not declare is ignored, which is what lets one map serve all stages.
    std::vector<VkSpecializationMapEntry> specEntries;
    std::vector<uint32_t> specData;
    specEntries.reserve(specializationConstants.size());
    specData.reserve(specializationConstants.size());
    for (const auto& [constantId, value] : specializationConstants) {
        specEntries.push_back({
            .constantID = constantId,
            .offset = static_cast<uint32_t>(specData.size() * sizeof(uint32_t)),
            .size = sizeof(uint32_t),
        });
        specData.push_back(toSpecializationBytes(value));
    }
    const VkSpecializationInfo specInfo{
        .mapEntryCount = static_cast<uint32_t>(specEntries.size()),
        .pMapEntries = specEntries.data(),
        .dataSize = specData.size() * sizeof(uint32_t),
        .pData = specData.data(),
    };
    const VkSpecializationInfo* const pSpecInfo = specEntries.empty() ? nullptr : &specInfo;

    for (const auto& [stageFlag, fileStem] : shaderFiles) {
        const auto absoluteSpvPath = spvShaderDir / (fileStem + ".spv");
        const auto spvFile = readSpirvFile(absoluteSpvPath).unwrap();
        auto stageInfo = createShaderStageInfo(stageFlag, shaderModules.create(spvFile, fileStem));
        stageInfo.pSpecializationInfo = pSpecInfo;
        builder.addShaderStage(stageInfo);
        shaderMetadata.merge(reflectPipelineLayoutFromSpirv(spvFile).unwrap());
        if (stageFlag == VK_SHADER_STAGE_VERTEX_BIT) {
            vertexInputMetadata = reflectVertexMetadataFromSpirvShader(spvFile).unwrap();
        }
    }

    if (shaderStagesMatchTessellation(shaderFiles)) {
        // Assume that we are dealing with quad tessellation for now.
        builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
    }

    const bool isMeshPipeline = shaderFiles.contains(VK_SHADER_STAGE_MESH_BIT_EXT);
    if (!isMeshPipeline && !hasField<JsonType::Array>(pipelineJson, "vertexInputBindings")) {
        return resultError("Pipeline field 'vertexInputBindings' must be an array.");
    }
    if (isMeshPipeline && shaderFiles.contains(VK_SHADER_STAGE_VERTEX_BIT)) {
        return resultError("Pipeline declares both a vertex and a mesh stage; they are mutually exclusive.");
    }
    if (pipelineJson.contains("vertexAttributes")) {
        return resultError(
            "Pipeline field 'vertexAttributes' is obsolete; vertex formats and attributes are reflected from the "
            "shader.");
    }
    if (!isMeshPipeline) {
        CRISP_TRY(
            readVertexInputBindings(pipelineJson["vertexInputBindings"], vertexInputMetadata, builder),
            "Invalid vertex input metadata");
    }

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
        readViewportState(pipelineJson["viewport"], builder).unwrap();
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
    if (pipelineJson.contains("descriptorSets")) {
        if (!pipelineJson["descriptorSets"].is_array()) {
            return resultError("Pipeline field 'descriptorSets' must be an array.");
        }
        CRISP_TRY(
            readDescriptorSetMetadata(pipelineJson["descriptorSets"], layoutBuilder, bindlessDescriptorSetLayout),
            "Invalid descriptor set metadata");
    }

    return builder.create(device, layoutBuilder.create(device), rasterizationPassDescriptor);
}

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromFileImpl(
    const std::filesystem::path& path,
    const std::filesystem::path& spvShaderDir,
    const VulkanDevice& device,
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor,
    const VkDescriptorSetLayout bindlessDescriptorSetLayout,
    const SpecializationConstantMap& specializationConstants) {
    CRISP_TRY(const auto& json, loadJsonFromFile(path), "Failed to open json config at {}", path.generic_string());
    CRISP_TRY(
        auto pipeline,
        createPipelineFromJson(
            json, spvShaderDir, device, rasterizationPassDescriptor, bindlessDescriptorSetLayout, specializationConstants),
        "Failed to create pipeline from json");
    device.setObjectName(*pipeline, fmt::format("{} Pipeline", path.stem().string()));
    return pipeline;
}

} // namespace

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromFile(
    const std::filesystem::path& path,
    const std::filesystem::path& spvShaderDir,
    const VulkanDevice& device,
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor,
    const VkDescriptorSetLayout bindlessDescriptorSetLayout,
    const SpecializationConstantMap& specializationConstants) {
    return createPipelineFromFileImpl(
        path, spvShaderDir, device, rasterizationPassDescriptor, bindlessDescriptorSetLayout, specializationConstants);
}

} // namespace crisp
