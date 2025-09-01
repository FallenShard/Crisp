
#include <Crisp/Scenes/VulkanRayTracingScene.hpp>

#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Image/Io/Exr.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Math/AliasTable.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>

namespace crisp {
namespace {

AliasTable createAliasTable(const TriangleMesh& mesh, bool addHeaderEntry = true) {
    std::vector<float> weights;
    weights.reserve(mesh.getTriangleCount());

    float totalArea = 0.0f;
    for (uint32_t i = 0; i < mesh.getTriangleCount(); ++i) {
        weights.push_back(mesh.calculateTriangleArea(i));
        totalArea += weights.back();
    }

    auto table = ::crisp::createAliasTable(weights);
    if (addHeaderEntry) {
        table.insert(table.begin(), {.tau = 1.0f / totalArea, .j = mesh.getTriangleCount()});
    }

    return table;
}

void append(AliasTable& globalAliasTable, const TriangleMesh& mesh) {
    const auto aliasTable = createAliasTable(mesh);
    globalAliasTable.insert(globalAliasTable.end(), aliasTable.begin(), aliasTable.end());
}

std::unique_ptr<VulkanBuffer> createAliasTableBuffer(Renderer& renderer, const AliasTable& aliasTable) {
    auto buffer = createStorageBuffer(renderer.getDevice(), aliasTable.size() * sizeof(AliasTable::value_type));
    fillDeviceBuffer(renderer, buffer.get(), aliasTable);
    return buffer;
}

Geometry createRayTracingGeometry(Renderer& renderer, const TriangleMesh& mesh) {
    return createGeometry(
        renderer,
        mesh,
        {{VertexAttribute::Position}, {VertexAttribute::Normal}},
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
}

void setCameraParameters(FreeCameraController& cameraController, const nlohmann::json& camera) {
    cameraController.setPosition(parseVec3(camera["position"]));
    cameraController.setFovY(camera["fovY"].get<float>());
}

} // namespace

VulkanRayTracingScene::VulkanRayTracingScene(Renderer* renderer, Window* window, std::filesystem::path outputDir)
    : Scene(renderer, window)
    , m_outputDir(std::move(outputDir)) {
    setupInput();

    const auto json =
        loadJsonFromFile(renderer->getAssetPaths().resourceDir / "VesperScenes/Nori-PA-4/cbox-mats.json").unwrap();
    m_sceneDesc = parseSceneDescription(json["shapes"]);

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    setCameraParameters(*m_cameraController, json["camera"]);
    m_resourceContext->createUniformRingBuffer<CameraParameters>("camera");

    m_integratorParams.shapeCount = static_cast<int32_t>(m_sceneDesc.meshFilenames.size());
    m_integratorParams.lightCount = static_cast<int32_t>(m_sceneDesc.lights.size());
    m_resourceContext->createUniformRingBuffer<IntegratorParameters>("integrator");

    m_sceneDesc.brdfs.push_back(createMicrofacetBrdf(glm::vec3(0.5f, 0.2f, 0.01f), 0.01f));

    m_resourceContext->createRingBufferFromStdVec("brdfParams", m_sceneDesc.brdfs);
    m_resourceContext->createRingBufferFromStdVec("lightParams", m_sceneDesc.lights);

    AliasTable aliasTable{};
    TriangleMesh sceneMesh{};
    for (auto&& [idx, meshName] : std::views::enumerate(m_sceneDesc.meshFilenames)) {
        const std::filesystem::path relativePath = std::filesystem::path("Meshes") / meshName;
        auto mesh{loadTriangleMesh(renderer->getResourcesPath() / relativePath).unwrap()};
        mesh.transform(m_sceneDesc.transforms[idx]);

        m_sceneDesc.props[idx].triangleOffset = sceneMesh.getTriangleCount();
        m_sceneDesc.props[idx].vertexOffset = sceneMesh.getVertexCount();
        m_sceneDesc.props[idx].triangleCount = mesh.getTriangleCount();

        if (m_sceneDesc.props[idx].lightId != -1) {
            m_sceneDesc.props[idx].aliasTableOffset = static_cast<uint32_t>(aliasTable.size());
            m_sceneDesc.props[idx].aliasTableCount = mesh.getTriangleCount();
            append(aliasTable, mesh);
        }

        sceneMesh.append(std::move(mesh));
    }

    auto& sceneGeometry =
        m_resourceContext->addGeometry("scene-geometry", createRayTracingGeometry(*m_renderer, sceneMesh));

    std::vector<VulkanAccelerationStructure*> blases;
    for (auto&& [idx, _] : std::views::enumerate(m_sceneDesc.meshFilenames)) {
        m_bottomLevelAccelStructures.push_back(
            std::make_unique<VulkanAccelerationStructure>(
                m_renderer->getDevice(),
                createAccelerationStructureGeometry(
                    sceneGeometry, m_sceneDesc.props[idx].triangleOffset * sizeof(glm::uvec3)),
                m_sceneDesc.props[idx].triangleCount,
                glm::mat4(1.0f)));
        blases.push_back(m_bottomLevelAccelStructures.back().get());
    }
    m_topLevelAccelStructure = std::make_unique<VulkanAccelerationStructure>(m_renderer->getDevice(), blases);
    m_resourceContext->addBuffer("aliasTable", createAliasTableBuffer(*m_renderer, aliasTable));

    m_resourceContext->createRingBufferFromStdVec("instanceProps", m_sceneDesc.props);

    m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer) {
        std::vector<VulkanAccelerationStructure*> blases;
        for (auto& blas : m_bottomLevelAccelStructures) {
            blas->build(cmdBuffer);
            blases.push_back(blas.get());
        }

        m_topLevelAccelStructure->build(cmdBuffer);
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
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_rayTracedImage = std::make_unique<VulkanImage>(m_renderer->getDevice(), createInfo);

    m_pipeline = createPipeline();

    m_material = std::make_unique<Material>(m_pipeline.get());

    updateDescriptorSets();

    m_renderer->setSceneImageView(&m_rayTracedImage->getView());
}

void VulkanRayTracingScene::resize(int /*width*/, int /*height*/) {
    /* m_cameraController->onViewportResized(width, height);

     m_renderGraph->resize(width, height);
     m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(),
     0);*/
}

void VulkanRayTracingScene::update(const UpdateParams& updateParams) {
    if (m_cameraController->update(updateParams.dt)) {
        m_integratorParams.frameIdx = 0;
    }
}

void VulkanRayTracingScene::render(const FrameContext& frameContext) {
    CRISP_TRACE_VK_SCOPE("VulkanRayTracingScene::render", frameContext.commandEncoder.getHandle());

    frameContext.commandEncoder.insertBarrier(kRayTracingRead >> kTransferWrite);

    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(
        m_cameraController->getCameraParameters(), frameContext.virtualFrameIndex);
    m_resourceContext->getRingBuffer("integrator")
        ->updateStagingBufferFromStruct(m_integratorParams, frameContext.virtualFrameIndex);
    m_resourceContext->getRingBuffer("brdfParams")
        ->updateStagingBufferFromStdVec(m_sceneDesc.brdfs, frameContext.virtualFrameIndex);
    m_resourceContext->getRingBuffer("lightParams")
        ->updateStagingBufferFromStdVec(m_sceneDesc.lights, frameContext.virtualFrameIndex);

    m_resourceContext->getRingBuffer("camera")->updateDeviceBuffer(frameContext.commandEncoder.getHandle());
    m_resourceContext->getRingBuffer("integrator")->updateDeviceBuffer(frameContext.commandEncoder.getHandle());
    m_resourceContext->getRingBuffer("brdfParams")->updateDeviceBuffer(frameContext.commandEncoder.getHandle());
    m_resourceContext->getRingBuffer("lightParams")->updateDeviceBuffer(frameContext.commandEncoder.getHandle());

    frameContext.commandEncoder.insertBarrier(kTransferWrite >> kRayTracingRead);

    if (m_screenshotBuffer && !m_screenshotRequestFrameIdx) {
        m_screenshotRequestFrameIdx = frameContext.frameIndex;
        frameContext.commandEncoder.transitionLayout(
            *m_rayTracedImage, VK_IMAGE_LAYOUT_GENERAL, kFragmentRead >> (kRayTracingStorageWrite | kTransferRead));
        frameContext.commandEncoder.copyImageToBuffer(*m_rayTracedImage, *m_screenshotBuffer);
    }

    frameContext.commandEncoder.transitionLayout(
        *m_rayTracedImage, VK_IMAGE_LAYOUT_GENERAL, kFragmentRead >> kRayTracingStorageWrite);
    m_pipeline->bind(frameContext.commandEncoder.getHandle());
    m_material->bind(frameContext.commandEncoder.getHandle());

    const auto extent = m_renderer->getSwapChainExtent();
    frameContext.commandEncoder.traceRays(m_shaderBindingTable.bindings, extent);
    frameContext.commandEncoder.transitionLayout(
        *m_rayTracedImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kRayTracingStorageWrite >> kFragmentRead);

    m_integratorParams.frameIdx++;

    if (m_screenshotRequestFrameIdx &&
        *m_screenshotRequestFrameIdx + Renderer::NumVirtualFrames == frameContext.frameIndex) {
        const std::span<const float> pixelData(
            m_screenshotBuffer->getHostVisibleData<float>(),
            m_rayTracedImage->getWidth() * m_rayTracedImage->getHeight() * 4);
        saveExr(m_outputDir / "screenshot.exr", pixelData, m_rayTracedImage->getWidth(), m_rayTracedImage->getHeight())
            .unwrap();
        m_screenshotRequestFrameIdx = std::nullopt;
        m_screenshotBuffer.reset();
    }
}

void VulkanRayTracingScene::drawGui() {
    ImGui::Begin("Integrator");
    ImGui::LabelText("Acc. Samples", "%d", m_integratorParams.frameIdx * m_integratorParams.sampleCount); // NOLINT
    if (ImGui::InputInt("Max Bounces", &m_integratorParams.maxBounces)) {
        m_integratorParams.frameIdx = 0;
    }
    if (ImGui::InputInt("Samples per Frame", &m_integratorParams.sampleCount)) {
        m_integratorParams.sampleCount = std::max(1, m_integratorParams.sampleCount);
        m_integratorParams.frameIdx = 0;
    }

    ImGui::Separator();

    if (!m_sceneDesc.lights.empty()) {
        if (ImGui::SliderFloat("Light R", &m_sceneDesc.lights[0].radiance[0], 0.0f, 50.0f)) {
            m_integratorParams.frameIdx = 0;
        }
        if (ImGui::SliderFloat("Light G", &m_sceneDesc.lights[0].radiance[1], 0.0f, 50.0f)) {
            m_integratorParams.frameIdx = 0;
        }
        if (ImGui::SliderFloat("Light B", &m_sceneDesc.lights[0].radiance[2], 0.0f, 50.0f)) {
            m_integratorParams.frameIdx = 0;
        }
    }
    if (m_sceneDesc.brdfs.size() > 5 && ImGui::SliderFloat("Int IOR", &m_sceneDesc.brdfs[5].intIor, 1.0f, 10.0f)) {
        m_integratorParams.frameIdx = 0;
    }

    ImGui::Separator();

    if (ImGui::RadioButton("Use Light Sampling", m_integratorParams.samplingMode == 0)) {
        m_integratorParams.samplingMode = 0;
        m_integratorParams.frameIdx = 0;
    }
    if (ImGui::RadioButton("Use BRDF Sampling", m_integratorParams.samplingMode == 1)) {
        m_integratorParams.samplingMode = 1;
        m_integratorParams.frameIdx = 0;
    }
    if (ImGui::RadioButton("Use MIS", m_integratorParams.samplingMode == 2)) {
        m_integratorParams.samplingMode = 2;
        m_integratorParams.frameIdx = 0;
    }
    if (ImGui::Button("Take Screenshot")) {
        const VkDeviceSize size = m_rayTracedImage->getWidth() * m_rayTracedImage->getHeight() * 4 * sizeof(float);
        m_screenshotBuffer =
            std::make_shared<StagingVulkanBuffer>(m_renderer->getDevice(), size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }

    ImGui::End();

    drawCameraUi(m_cameraController->getCamera());
}

std::unique_ptr<VulkanPipeline> VulkanRayTracingScene::createPipeline() {
    std::vector<std::pair<std::string, VkRayTracingShaderGroupTypeKHR>> shaderInfos{
        {"path-trace.rgen", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace.rmiss", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace.rchit", VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR},
        {"path-trace-lambertian.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace-dielectric.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace-mirror.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace-microfacet.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
    };
    std::vector<std::filesystem::path> shaderSpvPaths;
    shaderSpvPaths.reserve(shaderInfos.size());
    for (const auto& name : shaderInfos) {
        shaderSpvPaths.emplace_back(m_renderer->getAssetPaths().getShaderSpvPath(name.first));
    }
    PipelineLayoutBuilder builder{reflectPipelineLayoutFromSpirv(shaderSpvPaths).unwrap()};
    auto pipelineLayout = builder.create(m_renderer->getDevice());

    RayTracingPipelineBuilder pipelineBuilder(*m_renderer);
    for (auto&& [idx, info] : std::views::enumerate(shaderInfos)) {
        pipelineBuilder.addShaderStage(info.first);
        pipelineBuilder.addShaderGroup(static_cast<uint32_t>(idx), info.second);
    }

    const VkPipeline pipeline{pipelineBuilder.createHandle(pipelineLayout->getHandle())};
    m_shaderBindingTable = pipelineBuilder.createShaderBindingTable(pipeline);

    return std::make_unique<VulkanPipeline>(
        m_renderer->getDevice(), pipeline, std::move(pipelineLayout), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
}

void VulkanRayTracingScene::updateDescriptorSets() {
    m_material->writeDescriptor(0, 0, m_topLevelAccelStructure->getDescriptorInfo());
    m_material->writeDescriptor(0, 1, m_rayTracedImage->getView().getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    m_material->writeDescriptor(0, 2, *m_resourceContext->getRingBuffer("camera"));
    m_material->writeDescriptor(0, 3, *m_resourceContext->getRingBuffer("integrator"));
    m_material->writeDescriptor(
        1, 0, m_resourceContext->getGeometry("scene-geometry").getVertexBuffer()->createDescriptorInfo());
    m_material->writeDescriptor(
        1, 1, m_resourceContext->getGeometry("scene-geometry").getIndexBuffer()->createDescriptorInfo());
    m_material->writeDescriptor(
        1, 6, m_resourceContext->getGeometry("scene-geometry").getVertexBuffer(1)->createDescriptorInfo());
    m_material->writeDescriptor(1, 2, *m_resourceContext->getRingBuffer("instanceProps"));
    m_material->writeDescriptor(1, 3, *m_resourceContext->getRingBuffer("brdfParams"));
    m_material->writeDescriptor(1, 4, *m_resourceContext->getRingBuffer("lightParams"));
    m_material->writeDescriptor(1, 5, *m_resourceContext->getBuffer("aliasTable"));
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