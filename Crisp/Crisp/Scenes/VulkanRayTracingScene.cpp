
#include <Crisp/Scenes/VulkanRayTracingScene.hpp>

#include <Crisp/IO/JsonUtils.hpp>
#include <Crisp/Math/AliasTable.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>

#include <Crisp/GUI/ImGuiUtils.hpp>

namespace crisp {
namespace {

BrdfParameters createLambertianBrdf(glm::vec3 albedo) {
    return {
        .albedo = albedo,
        .type = 0,
    };
}

BrdfParameters createDielectricBrdf(const float intIor) {
    return {
        .type = 1,
        .intIor = intIor,
    };
}

BrdfParameters createMirrorBrdf() {
    return {
        .type = 2,
    };
}

BrdfParameters createMicrofacetBrdf(const glm::vec3 kd, const float alpha) {
    return {
        .type = 3,
        .intIor = Fresnel::getIOR(IndexOfRefraction::Glass),
        .kd = kd,
        .ks = 1.0f - std::max(kd.x, std::max(kd.y, kd.z)),
        .microfacetAlpha = alpha,
    };
}

AliasTable createAliasTable(const TriangleMesh& mesh) {
    std::vector<float> weights;
    weights.reserve(mesh.getFaceCount());

    float totalArea = 0.0f;
    for (uint32_t i = 0; i < mesh.getFaceCount(); ++i) {
        weights.push_back(mesh.calculateFaceArea(i));
        totalArea += weights.back();
    }

    auto table = ::crisp::createAliasTable(weights);
    table.insert(table.begin(), {.tau = 1.0f / totalArea, .j = mesh.getFaceCount()});
    return table;
}

std::unique_ptr<StorageBuffer> createAliasTableBuffer(Renderer& renderer, const TriangleMesh& mesh) {
    const auto aliasTable = createAliasTable(mesh);
    return std::make_unique<StorageBuffer>(
        &renderer,
        aliasTable.size() * sizeof(AliasTable::value_type),
        0,
        BufferUpdatePolicy::Constant,
        aliasTable.data());
}

const VertexLayoutDescription posFormat = {{VertexAttribute::Position, VertexAttribute::Normal}};

std::unique_ptr<Geometry> createRayTracingGeometry(Renderer& renderer, TriangleMesh&& mesh) {
    return std::make_unique<Geometry>(
        renderer,
        std::move(mesh),
        posFormat,
        /*padToVec4=*/false,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
}

VkAccelerationStructureGeometryKHR createAccelerationStructureGeometry(const Geometry& geometry) {
    VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geo.geometry = {};
    geo.geometry.triangles = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    geo.geometry.triangles.vertexData.deviceAddress = geometry.getVertexBuffer()->getDeviceAddress();
    geo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; // Positions format.
    geo.geometry.triangles.vertexStride = 2 * sizeof(glm::vec3);      // Spacing between positions.
    geo.geometry.triangles.maxVertex = geometry.getVertexCount() - 1;
    geo.geometry.triangles.indexData.deviceAddress = geometry.getIndexBuffer()->getDeviceAddress();
    geo.geometry.triangles.indexType = geometry.getIndexType();
    return geo;
}

glm::vec3 parseVec3(const nlohmann::json& json) {
    return {json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
}

std::vector<BrdfParameters> createBrdfs(const nlohmann::json& shapeList) {
    std::vector<BrdfParameters> brdfs{};
    for (const auto& shape : shapeList) {
        const auto& brdf{shape["brdf"]};
        const auto& type{brdf["type"]};
        if (type == "lambertian") {
            brdfs.push_back(createLambertianBrdf(parseVec3(brdf["albedo"])));
        } else if (type == "dielectric") {
            brdfs.push_back(createDielectricBrdf(brdf["intIOR"].get<float>()));
        } else if (type == "mirror") {
            brdfs.push_back(createMirrorBrdf());
        }
    }

    return brdfs;
}

std::vector<std::string> getMeshFilenames(const nlohmann::json& shapeList) {
    std::vector<std::string> filenames{};
    for (const auto& shape : shapeList) {
        filenames.push_back(shape["path"]);
    }
    return filenames;
}

std::vector<glm::mat4> getTransforms(const nlohmann::json& shapeList) {
    std::vector<glm::mat4> transforms{};
    for (const auto& shape : shapeList) {
        const glm::mat4 translation =
            shape.contains("translation") ? glm::translate(parseVec3(shape["translation"])) : glm::mat4(1.0f);
        const glm::mat4 rotation = glm::mat4(1.0f);
        const glm::mat4 scale =
            shape.contains("scale") ? glm::scale(glm::vec3(shape["scale"].get<float>())) : glm::mat4(1.0f);

        transforms.push_back(translation * rotation * scale);
    }
    return transforms;
}

} // namespace

VulkanRayTracingScene::VulkanRayTracingScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {
    setupInput();

    auto json =
        loadJsonFromFile(renderer->getAssetPaths().resourceDir / "VesperScenes/Nori-PA-4/cbox-mats.json").unwrap();

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_cameraController->setPosition(parseVec3(json["camera"]["position"]));
    m_cameraController->setFovY(json["camera"]["fovY"].get<float>());
    m_resourceContext->createUniformBuffer("camera", sizeof(ExtendedCameraParameters), BufferUpdatePolicy::PerFrame);
    m_resourceContext->createUniformBuffer("integrator", sizeof(IntegratorParameters), BufferUpdatePolicy::PerFrame);

    const std::vector<std::string> meshNames = getMeshFilenames(json["shapes"]);
    const std::vector<uint32_t> materialIds = {0, 1, 2, 3, 4, 6};
    m_brdfParameters = createBrdfs(json["shapes"]);
    m_brdfParameters.push_back(createMicrofacetBrdf(glm::vec3(0.5f, 0.2f, 0.01f), 0.01f));

    m_resourceContext->createStorageBuffer(
        "brdfParams",
        sizeof(BrdfParameters) * m_brdfParameters.size(),
        0,
        BufferUpdatePolicy::PerFrame,
        m_brdfParameters.data());

    m_resourceContext->createStorageBuffer(
        "materialIds", sizeof(uint32_t) * meshNames.size(), 0, BufferUpdatePolicy::Constant, materialIds.data());

    const auto transforms = getTransforms(json["shapes"]);
    std::vector<VulkanAccelerationStructure*> blases;
    for (auto&& [idx, meshName] : std::views::enumerate(meshNames)) {
        const std::filesystem::path relativePath = std::filesystem::path("Meshes") / meshName;

        auto mesh{loadTriangleMesh(renderer->getResourcesPath() / relativePath, flatten(posFormat)).unwrap()};
        mesh.transform(transforms[idx]);

        if (idx == 3) {
            m_resourceContext->addStorageBuffer("aliasTable", createAliasTableBuffer(*m_renderer, mesh));
        }

        auto& geometry = m_resourceContext->addGeometry(
            fmt::format("{}_{}", meshName, idx), createRayTracingGeometry(*m_renderer, std::move(mesh)));
        m_bottomLevelAccelStructures.push_back(std::make_unique<VulkanAccelerationStructure>(
            m_renderer->getDevice(),
            createAccelerationStructureGeometry(geometry),
            geometry.getIndexCount() / 3,
            glm::mat4(1.0f)));

        blases.push_back(m_bottomLevelAccelStructures.back().get());
    }
    m_topLevelAccelStructure = std::make_unique<VulkanAccelerationStructure>(m_renderer->getDevice(), blases);

    m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer) {
        std::vector<VulkanAccelerationStructure*> blases;
        for (auto& blas : m_bottomLevelAccelStructures) {
            blas->build(m_renderer->getDevice(), cmdBuffer, {});
            blases.push_back(blas.get());
        }

        m_topLevelAccelStructure->build(m_renderer->getDevice(), cmdBuffer, blases);
    });

    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    createInfo.extent = m_renderer->getSwapChainExtent3D();
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_rtImage = std::make_unique<VulkanImage>(m_renderer->getDevice(), createInfo);

    for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i) {
        m_rtImageViews.emplace_back(createView(*m_rtImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    }

    m_pipeline = createPipeline();

    m_material = std::make_unique<Material>(m_pipeline.get());

    updateDescriptorSets();
    for (auto&& [idx, name] : std::views::enumerate(meshNames)) {
        updateGeometryBufferDescriptors(
            *m_resourceContext->getGeometry(fmt::format("{}_{}", name, idx)), static_cast<uint32_t>(idx));
    }

    m_renderer->getDevice().flushDescriptorUpdates();

    m_renderer->setSceneImageViews(m_rtImageViews);
} // namespace crisp

void VulkanRayTracingScene::resize(int /*width*/, int /*height*/) {
    /* m_cameraController->onViewportResized(width, height);

     m_renderGraph->resize(width, height);
     m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(),
     0);*/
}

void VulkanRayTracingScene::update(float dt) {
    if (m_cameraController->update(dt)) {
        m_integratorParams.frameIdx = 0;
    }

    const ExtendedCameraParameters cameraParams = m_cameraController->getExtendedCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);
    m_resourceContext->getUniformBuffer("integrator")->updateStagingBuffer(m_integratorParams);
    m_resourceContext->getStorageBuffer("brdfParams")->updateStagingBufferFromVector(m_brdfParameters);
}

void VulkanRayTracingScene::render() {
    m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) {
        m_rtImage->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            0,
            1,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

        m_pipeline->bind(cmdBuffer);
        m_material->bind(m_renderer->getCurrentVirtualFrameIndex(), cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

        const auto extent = m_renderer->getSwapChainExtent();
        vkCmdTraceRaysKHR(
            cmdBuffer,
            &m_shaderBindingTable.bindings[ShaderBindingTable::kRayGen],
            &m_shaderBindingTable.bindings[ShaderBindingTable::kMiss],
            &m_shaderBindingTable.bindings[ShaderBindingTable::kHit],
            &m_shaderBindingTable.bindings[ShaderBindingTable::kCall],
            extent.width,
            extent.height,
            1);

        m_rtImage->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            1,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        m_integratorParams.frameIdx++;
    });
}

void VulkanRayTracingScene::renderGui() {
    ImGui::Begin("Integrator");
    ImGui::LabelText("Acc. Samples", "%d", m_integratorParams.frameIdx * m_integratorParams.sampleCount); // NOLINT
    if (ImGui::InputInt("Max Bounces", &m_integratorParams.maxBounces)) {
        m_integratorParams.frameIdx = 0;
    }
    if (ImGui::InputInt("Samples per Frame", &m_integratorParams.sampleCount)) {
        m_integratorParams.frameIdx = 0;
    }
    if (ImGui::SliderFloat("Int IOR", &m_brdfParameters[4].intIor, 1.0f, 10.0f)) {
        m_integratorParams.frameIdx = 0;
    }
    if (ImGui::SliderFloat("Roughness IOR", &m_brdfParameters[6].microfacetAlpha, 1e-3f, 1.0f)) {
        m_integratorParams.frameIdx = 0;
    }

    ImGui::End();
}

std::unique_ptr<VulkanPipeline> VulkanRayTracingScene::createPipeline() {
    std::vector<std::pair<std::string, VkRayTracingShaderGroupTypeKHR>> shaderInfos{
        {"path-trace.rgen", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace.rmiss", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace.rchit", VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR},
        // {"path-trace-shadow-ray.rahit", VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR},
        {"path-trace-lambertian.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace-dielectric.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace-mirror.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace-microfacet.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
    };
    std::vector<std::filesystem::path> shaderSpvPaths;
    shaderSpvPaths.reserve(shaderInfos.size());
    for (const auto& name : shaderInfos) {
        shaderSpvPaths.emplace_back(m_renderer->getAssetPaths().getSpvShaderPath(name.first));
    }
    PipelineLayoutBuilder builder{reflectUniformMetadataFromSpirvPaths(shaderSpvPaths).unwrap()};
    builder.setDescriptorSetBuffering(0, true);
    builder.setDescriptorDynamic(0, 2, true);
    builder.setDescriptorDynamic(0, 3, true);
    builder.setDescriptorDynamic(1, 3, true);
    auto pipelineLayout = builder.create(m_renderer->getDevice());

    RayTracingPipelineBuilder pipelineBuilder(*m_renderer);
    for (auto&& [idx, info] : std::views::enumerate(shaderInfos)) {
        pipelineBuilder.addShaderStage(info.first);
        pipelineBuilder.addShaderGroup(static_cast<uint32_t>(idx), info.second);
    }

    const VkPipeline pipeline{pipelineBuilder.createHandle(pipelineLayout->getHandle())};
    m_shaderBindingTable = pipelineBuilder.createShaderBindingTable(pipeline);

    return std::make_unique<VulkanPipeline>(
        m_renderer->getDevice(),
        pipeline,
        std::move(pipelineLayout),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        VertexLayout{});
}

void VulkanRayTracingScene::updateDescriptorSets() {
    m_material->writeDescriptor(0, 0, m_topLevelAccelStructure->getDescriptorInfo());
    m_material->writeDescriptor(0, 1, m_rtImageViews, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    m_material->writeDescriptor(0, 2, *m_resourceContext->getUniformBuffer("camera"));
    m_material->writeDescriptor(0, 3, *m_resourceContext->getUniformBuffer("integrator"));
    m_material->writeDescriptor(1, 2, *m_resourceContext->getStorageBuffer("materialIds"));
    m_material->writeDescriptor(1, 3, *m_resourceContext->getStorageBuffer("brdfParams"));
    m_material->writeDescriptor(1, 4, *m_resourceContext->getStorageBuffer("aliasTable"));
    m_renderer->getDevice().flushDescriptorUpdates();
}

void VulkanRayTracingScene::updateGeometryBufferDescriptors(const Geometry& geometry, uint32_t idx) {
    m_material->writeDescriptor(1, 0, geometry.getVertexBuffer()->createDescriptorInfo(), idx);
    m_material->writeDescriptor(1, 1, geometry.getIndexBuffer()->createDescriptorInfo(), idx);
    m_renderer->getDevice().flushDescriptorUpdates();
}

void VulkanRayTracingScene::setupInput() {
    m_window->keyPressed += [this](Key key, int) {
        switch (key) {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        default: {
        }
        }
    };
}

} // namespace crisp