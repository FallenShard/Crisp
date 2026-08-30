
#include <Crisp/Scenes/VulkanRayTracingScene.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Image/Io/Exr.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Math/AliasTable.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/ShaderUtils/Reflection.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {

template <typename T>
std::span<const std::byte> structAsBytes(const T& value) {
    return std::span<const std::byte>{reinterpret_cast<const std::byte*>(&value), sizeof(value)}; // NOLINT
}

struct PathTracingPassData {
    RenderGraphResourceHandle image;
};

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
    auto buffer = createStorageBuffer(
        renderer.getDevice(),
        aliasTable.size() * sizeof(AliasTable::value_type),
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
    fillDeviceBuffer(renderer, buffer.get(), aliasTable);
    return buffer;
}

Geometry createRayTracingGeometry(Renderer& renderer, const TriangleMesh& mesh) {
    return createGeometry(
        renderer,
        mesh,
        {{VertexAttribute::Position}, {VertexAttribute::Normal}},
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
}

void setCameraParameters(FreeCameraController& cameraController, const nlohmann::json& camera) {
    cameraController.setPosition(parseVec3(camera["position"]));
    cameraController.setFovY(camera["fovY"].get<float>());
}

// Must match the heap array subscripts in Shaders/path-trace.rgen.glsl. The BVH slot is reached through a
// (set, binding) mapping rather than a subscript, so its number is private to this file.
constexpr uint32_t kBvhSlot = 0;
constexpr uint32_t kImageSlot = 1;
constexpr uint32_t kViewSlot = 2;
constexpr uint32_t kIntegratorSlot = 3;
constexpr uint32_t kHeapSlotCount = 4;

} // namespace

VulkanRayTracingScene::VulkanRayTracingScene(
    Renderer* renderer, Window* window, std::filesystem::path outputDir, const nlohmann::json& args)
    : Scene(renderer, window)
    , m_outputDir(std::move(outputDir)) {
    setupInput();

    m_integratorParams.sampleCount = std::max(1, args.value("samplesPerFrame", 1));
    m_captureAfterSamples = std::max(0, args.value("captureAfterSamples", 0));
    m_closeAfterScreenshot = args.value("closeAfterCapture", false);
    m_screenshotFilename = args.value("captureFilename", std::string{"screenshot.exr"});

    const auto sceneFile = args.value("sceneFile", std::string{"VesperScenes/Nori-PA-4/cbox-mats.json"});
    const auto json = loadJsonFromFile(renderer->getAssetPaths().resourceDir / sceneFile).unwrap();
    m_sceneDesc = parseSceneDescription(json["shapes"]);

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    setCameraParameters(*m_cameraController, json["camera"]);
    m_cameraBuffer = m_resourceContext->createUniformBuffer<CameraParameters>("camera");

    m_integratorParams.shapeCount = static_cast<int32_t>(m_sceneDesc.meshFilenames.size());
    m_integratorParams.lightCount = static_cast<int32_t>(m_sceneDesc.lights.size());
    m_integratorBuffer = m_resourceContext->createUniformBuffer<IntegratorParameters>("integrator");

    m_sceneDesc.brdfs.push_back(createMicrofacetBrdf(glm::vec3(0.5f, 0.2f, 0.01f), 0.01f));

    m_brdfParamsBuffer = m_resourceContext->createStorageBuffer(
        "brdfParams", m_sceneDesc.brdfs, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
    m_lightParamsBuffer = m_resourceContext->createStorageBuffer(
        "lightParams", m_sceneDesc.lights, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

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
        m_bottomLevelAccelStructures.back()->setDebugName(
            m_renderer->getDevice(), fmt::format("Path Tracer BLAS [{}]", m_sceneDesc.meshFilenames[idx]));
        blases.push_back(m_bottomLevelAccelStructures.back().get());
    }
    m_topLevelAccelStructure = std::make_unique<VulkanAccelerationStructure>(m_renderer->getDevice(), blases);
    m_topLevelAccelStructure->setDebugName(m_renderer->getDevice(), "Path Tracer TLAS");
    m_aliasTableBuffer = m_resourceContext->addBuffer("aliasTable", createAliasTableBuffer(*m_renderer, aliasTable));

    m_instancePropsBuffer = m_resourceContext->createStorageBuffer(
        "instanceProps", m_sceneDesc.props, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

    m_sceneAddresses = {
        .vertices = sceneGeometry.getVertexBuffer()->getDeviceAddress(),
        .normals = sceneGeometry.getVertexBuffer(1)->getDeviceAddress(),
        .triangles = sceneGeometry.getIndexBuffer()->getDeviceAddress(),
        .instances = m_instancePropsBuffer->getDeviceAddress(),
        .materials = m_brdfParamsBuffer->getDeviceAddress(),
        .lights = m_lightParamsBuffer->getDeviceAddress(),
        .aliasTable = m_aliasTableBuffer->getDeviceAddress(),
    };
    CRISP_CHECK_LE(
        sizeof(m_sceneAddresses),
        m_renderer->getPhysicalDevice().getDescriptorHeapProperties().maxPushDataSize,
        "Ray-tracing scene addresses exceed the descriptor-heap push-data limit.");

    m_renderer->enqueueResourceUpdate([this](const VulkanCommandEncoder& encoder) {
        std::vector<VulkanAccelerationStructure*> blases;
        for (auto& blas : m_bottomLevelAccelStructures) {
            encoder.buildAccelerationStructure(*blas);
            blases.push_back(blas.get());
        }

        encoder.buildAccelerationStructure(*m_topLevelAccelStructure);
    });

    m_descriptorHeap = std::make_unique<VulkanDescriptorHeap>(
        m_renderer->getDevice(), kHeapSlotCount, "Path Tracer Resource Descriptor Heap");
    m_pipeline = createPipeline();

    buildRenderGraph();
}

void VulkanRayTracingScene::buildRenderGraph() {
    m_renderGraph = std::make_unique<rg::RenderGraph>();
    m_renderGraph->addPass(
        "path-trace",
        PassType::RayTracing,
        [](rg::RenderGraph::Builder& builder) {
            builder.getBlackboard().insert<PathTracingPassData>().image = builder.createStorageImage(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                    // Screenshots copy straight out of the accumulation image.
                    .imageUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                },
                "path-trace-accumulation");
        },
        [this](const FrameContext& frameContext) { traceRays(frameContext); });
    m_renderGraph->exportTexture(m_renderGraph->getBlackboard().get<PathTracingPassData>().image);
    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    updateDescriptorHeap();
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&PathTracingPassData::image>());
}

void VulkanRayTracingScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    updateDescriptorHeap();
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&PathTracingPassData::image>());
    m_integratorParams.frameIdx = 0;
}

void VulkanRayTracingScene::update(const UpdateParams& updateParams) {
    if (m_cameraController->update(updateParams.dt)) {
        m_integratorParams.frameIdx = 0;
    }
}

void VulkanRayTracingScene::render(const FrameContext& frameContext) {
    CRISP_TRACE_VK_SCOPE("VulkanRayTracingScene::render", frameContext.commandEncoder);

    frameContext.commandEncoder.insertBarrier(kRayTracingRead >> kTransferWrite);

    const auto& cameraParams = m_cameraController->getCameraParameters();
    frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_cameraBuffer, 0, cameraParams);
    frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_integratorBuffer, 0, m_integratorParams);
    frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_brdfParamsBuffer, 0, m_sceneDesc.brdfs);
    frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_lightParamsBuffer, 0, m_sceneDesc.lights);

    frameContext.commandEncoder.insertBarrier(kTransferWrite >> kRayTracingRead);

    m_renderGraph->execute(frameContext);

    m_integratorParams.frameIdx++;

    const int64_t accumulatedSamples =
        static_cast<int64_t>(m_integratorParams.frameIdx) * m_integratorParams.sampleCount;
    if (m_captureAfterSamples > 0 && accumulatedSamples >= m_captureAfterSamples) {
        m_screenshotRequested = true;
        m_captureAfterSamples = 0;
    }

    if (const auto pixelData = m_screenshot.tryRead<float>(frameContext.completedValue)) {
        const auto extent =
            m_renderGraph->getImageExtent(m_renderGraph->getBlackboard().get<PathTracingPassData>().image);
        saveExr(m_outputDir / m_screenshotFilename, *pixelData, extent.width, extent.height).unwrap();
        m_screenshot.reset();
        if (m_closeAfterScreenshot) {
            m_window->close();
        }
    }
}

void VulkanRayTracingScene::traceRays(const FrameContext& frameContext) {
    const auto& encoder = frameContext.commandEncoder;
    uploadIfPending(*m_descriptorHeap, encoder, *frameContext.stagingBelt, kRayTracingResourceHeapRead);
    encoder.bindPipeline(*m_pipeline);
    encoder.bindDescriptorHeap(*m_descriptorHeap);
    encoder.pushData(structAsBytes(m_sceneAddresses));
    encoder.traceRays(m_shaderBindingTable.bindings, m_renderer->getSwapChainExtent());

    if (!m_screenshotRequested || m_screenshot.isPending()) {
        return;
    }

    // Read back from inside the pass, where the image is still VK_IMAGE_LAYOUT_GENERAL. That layout
    // is a legal transfer source, so no transition is needed and the graph's tracked layout stays
    // valid; only the access scopes have to be ordered, including the write-after-read against the
    // next frame's accumulation.
    const auto& image = m_renderGraph->getImageView<&PathTracingPassData::image>().getImage();
    encoder.insertBarrier(kRayTracingStorageWrite >> kTransferRead);

    const VkDeviceSize size = static_cast<VkDeviceSize>(image.getWidth()) * image.getHeight() * 4 * sizeof(float);
    m_screenshot.record(
        frameContext.stagingBelt->downloadImage(encoder, image, {image.getWidth(), image.getHeight(), 1u}, 0, 1, 0, size),
        frameContext.completionValue);
    m_screenshotRequested = false;

    encoder.insertBarrier(kTransferRead >> kRayTracingStorageWrite);
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
        m_screenshotRequested = true;
    }

    ImGui::End();

    drawCameraUi(m_cameraController->getCamera());

    ImGui::SetNextWindowSize(ImVec2(440.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Render Graph")) {
        drawRenderGraphGui(*m_renderGraph);
    }
    ImGui::End();
}

std::unique_ptr<VulkanPipeline> VulkanRayTracingScene::createPipeline() {
    std::vector<std::pair<std::string, VkRayTracingShaderGroupTypeKHR>> shaderInfos{
        {"path-trace.rgen", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace.rmiss", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"path-trace.rchit", VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR},
        {"Brdf/path-trace-lambertian.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"Brdf/path-trace-dielectric.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"Brdf/path-trace-mirror.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"Brdf/path-trace-microfacet.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"Brdf/path-trace-oren-nayar.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"Brdf/path-trace-smooth-conductor.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"Brdf/path-trace-rough-conductor.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"Brdf/path-trace-rough-dielectric.rcall", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
    };
    RayTracingPipelineBuilder pipelineBuilder(m_renderer->getDevice());
    for (auto&& [idx, info] : std::views::enumerate(shaderInfos)) {
        pipelineBuilder.addShaderStage(m_renderer->getAssetPaths().getShaderSpvPath(info.first));
        pipelineBuilder.addShaderGroup(static_cast<uint32_t>(idx), info.second);
    }

    const VkDescriptorSetAndBindingMappingEXT bvhMapping{
        m_descriptorHeap->makeMapping(kBvhSlot, 1, 0, VK_SPIRV_RESOURCE_TYPE_ACCELERATION_STRUCTURE_BIT_EXT)};
    pipelineBuilder.setDescriptorHeapMappings(0, {&bvhMapping, 1});

    const VkPipeline pipeline{pipelineBuilder.createDescriptorHeapHandle()};
    m_shaderBindingTable = pipelineBuilder.createShaderBindingTable(pipeline);
    m_renderer->getDevice().setObjectName(*m_shaderBindingTable.buffer, "Path Tracer Shader Binding Table");

    auto result = std::make_unique<VulkanPipeline>(
        m_renderer->getDevice(), pipeline, nullptr, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
    result->setDebugName(m_renderer->getDevice(), "Path Tracer");
    return result;
}

void VulkanRayTracingScene::updateDescriptorHeap() {
    m_descriptorHeap->writeAccelerationStructure(kBvhSlot, *m_topLevelAccelStructure);
    m_descriptorHeap->writeStorageImage(
        kImageSlot, m_renderGraph->getImageView<&PathTracingPassData::image>(), VK_IMAGE_LAYOUT_GENERAL);
    m_descriptorHeap->writeUniformBuffer(kViewSlot, *m_cameraBuffer);
    m_descriptorHeap->writeUniformBuffer(kIntegratorSlot, *m_integratorBuffer);
}

void VulkanRayTracingScene::setupInput() {
    m_window->keyPressed += [this](Key key, int) {
        switch (key) {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            m_integratorParams.frameIdx = 0;
            break;
        default: {
        }
        }
    };
}

} // namespace crisp
