
#include <Crisp/Scenes/VulkanRayTracingScene.hpp>

#include <array>
#include <cstring>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Geometry/VertexLayout.hpp>
#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Image/Io/Exr.hpp>
#include <Crisp/Image/Io/Utils.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Math/AliasTable.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Scenes/EnvironmentLightSampling.hpp>
#include <Crisp/ShaderUtils/Reflection.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {

struct PathTracingPassData {
    RenderGraphResourceHandle image;
};

// The header entry at index 0 carries the inverse total area and the triangle count, so the sampler needs
// nothing but the table's own address.
AliasTable createAliasTable(const TriangleMesh& mesh) {
    std::vector<float> weights;
    weights.reserve(mesh.getTriangleCount());

    float totalArea = 0.0f;
    for (uint32_t i = 0; i < mesh.getTriangleCount(); ++i) {
        weights.push_back(mesh.calculateTriangleArea(i));
        totalArea += weights.back();
    }

    auto table = ::crisp::createAliasTable(weights);
    table.insert(table.begin(), {.tau = 1.0f / totalArea, .j = mesh.getTriangleCount()});
    return table;
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
        kPbrVertexFormat,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
}

void setCameraParameters(FreeCameraController& cameraController, const RayTracingRenderSettings& settings) {
    cameraController.onViewportResized(settings.resolution.x, settings.resolution.y);
    cameraController.setLookAt(settings.cameraPosition, settings.cameraTarget, settings.cameraUp);
    cameraController.setFovY(settings.verticalFov);
    cameraController.setViewDepthRange(settings.zNear, settings.zFar);
}

Image loadEnvironmentImage(const std::filesystem::path& path) {
    if (path.extension() != ".exr") {
        return loadImage(path, 4, FlipAxis::None).unwrap();
    }

    const auto exr = loadExr(path).unwrap();
    const size_t pixelCount = static_cast<size_t>(exr.width) * exr.height;
    std::vector<float> rgba(pixelCount * 4, 1.0f);
    for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
        for (uint32_t channel = 0; channel < std::min(exr.channelCount, 4u); ++channel) {
            rgba[pixel * 4 + channel] = exr.pixelData[pixel * exr.channelCount + channel];
        }
        if (exr.channelCount == 1) {
            rgba[pixel * 4 + 1] = rgba[pixel * 4];
            rgba[pixel * 4 + 2] = rgba[pixel * 4];
        }
    }

    std::vector<uint8_t> bytes(rgba.size() * sizeof(float));
    std::memcpy(bytes.data(), rgba.data(), bytes.size());
    return {std::move(bytes), exr.width, exr.height, 4, 4 * sizeof(float)};
}

Image createConstantEnvironmentImage(const glm::vec3 radiance) {
    const std::array<float, 4> rgba{radiance.r, radiance.g, radiance.b, 1.0f};
    std::vector<uint8_t> bytes(sizeof(rgba));
    std::memcpy(bytes.data(), rgba.data(), sizeof(rgba));
    return {std::move(bytes), 1, 1, 4, 4 * sizeof(float)};
}

constexpr std::array<PathTracerShaderStage, 3> kShaderStages{{
    {"PathTracer/trace.rgen", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
    {"PathTracer/trace.rmiss", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
    {"PathTracer/trace.rchit", VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR},
}};

} // namespace

VulkanRayTracingScene::VulkanRayTracingScene(
    Renderer* renderer, Window* window, std::filesystem::path outputDir, const nlohmann::json& args)
    : Scene(renderer, window)
    , m_outputDir(std::move(outputDir)) {
    setupInput();

    m_samplesPerFrame = std::max(1, args.value("samplesPerFrame", 1));
    m_closeAfterScreenshot = args.value("closeAfterCapture", false);
    m_screenshotFilename = args.value("captureFilename", std::string{"screenshot.exr"});

    const auto scenePath =
        args.value("scenePath", std::string{"../tools/path_tracer_evaluation/crisp/cornell_box.json"});
    const auto json = loadJsonFromFile(renderer->getAssetPaths().resourceDir / scenePath).unwrap();
    const auto renderSettings = parseRayTracingRenderSettings(json).unwrap();
    m_renderResolution = renderSettings.resolution;
    m_integratorParams.maxBounces = renderSettings.maxDepth;
    m_integratorParams.seed = renderSettings.seed;
    m_integratorParams.samplingMode = renderSettings.samplingMode;
    m_integratorParams.reconstructionFilter = static_cast<int32_t>(renderSettings.reconstructionFilter);
    m_captureAfterSamples = m_closeAfterScreenshot ? renderSettings.samplesPerPixel : 0;
    m_sceneDesc = parseSceneDescription(json["shapes"], json.value("lights", nlohmann::json::array())).unwrap();

    m_materialImages.reserve(m_sceneDesc.materialTextures.size());
    for (const auto& texture : m_sceneDesc.materialTextures) {
        auto texturePath = renderer->getResourcesPath() / texture.filename;
        if (!std::filesystem::exists(texturePath)) {
            texturePath = renderer->getResourcesPath() / "Textures" / texture.filename;
        }
        const Image image =
            texturePath.extension() == ".exr"
                ? loadEnvironmentImage(texturePath)
                : loadImage(texturePath, 4, FlipAxis::None).unwrap();
        const VkFormat format =
            image.getPixelByteSize() == 4 * sizeof(float) ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_SRGB;
        m_materialImages.push_back(createVulkanImage(*renderer, image, format));
    }
    for (auto& material : m_sceneDesc.bsdfs) {
        if (material.reflectanceTexture < 0) {
            continue;
        }
        material.reflectanceTexture += static_cast<int32_t>(kPathTracerMaterialTextureFirstSlot);
        material.reflectanceSampler = static_cast<int32_t>(kPathTracerMaterialSamplerSlot);
    }

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    setCameraParameters(*m_cameraController, renderSettings);

    m_integratorParams.shapeCount = static_cast<int32_t>(m_sceneDesc.meshFilenames.size());
    m_integratorParams.environmentEnabled = m_sceneDesc.environment.has_value() ? 1 : 0;
    m_integratorParams.lightCount =
        static_cast<int32_t>(m_sceneDesc.lights.size()) + m_integratorParams.environmentEnabled;

    m_sceneDesc.bsdfs.push_back(createMicrofacetBsdf(glm::vec3(0.5f, 0.2f, 0.01f), 0.01f));

    m_bsdfParamsBuffer = m_resourceContext->createStorageBuffer(
        "bsdfParams", m_sceneDesc.bsdfs, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
    const std::vector<LightParameters> gpuLights =
        m_sceneDesc.lights.empty() ? std::vector{LightParameters{}} : m_sceneDesc.lights;
    m_lightParamsBuffer =
        m_resourceContext->createStorageBuffer("lightParams", gpuLights, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

    if (m_sceneDesc.environment) {
        const Image environmentImage = [&]() {
            if (m_sceneDesc.environment->radiance) {
                return createConstantEnvironmentImage(*m_sceneDesc.environment->radiance);
            }
            auto environmentPath = renderer->getResourcesPath() / *m_sceneDesc.environment->filename;
            if (!std::filesystem::exists(environmentPath)) {
                environmentPath = renderer->getResourcesPath() / "Textures" / *m_sceneDesc.environment->filename;
            }
            return loadEnvironmentImage(environmentPath);
        }();
        const auto pixelCount = static_cast<size_t>(environmentImage.getWidth()) * environmentImage.getHeight();
        const auto distribution = createEnvironmentSamplingDistribution(
            std::span<const float>{reinterpret_cast<const float*>(environmentImage.getData()), pixelCount * 4}, // NOLINT
            environmentImage.getWidth(),
            environmentImage.getHeight());

        m_integratorParams.environmentWidth = static_cast<int32_t>(environmentImage.getWidth());
        m_integratorParams.environmentHeight = static_cast<int32_t>(environmentImage.getHeight());
        m_integratorParams.environmentScale = m_sceneDesc.environment->radianceScale;
        m_environmentImage = createVulkanImage(*renderer, environmentImage, VK_FORMAT_R32G32B32A32_SFLOAT);
        m_environmentCdfBuffer = m_resourceContext->createStorageBuffer(
            "environmentCdf", distribution.getCdf(), VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
    }

    std::vector<PathTracerInstance> instances;
    instances.reserve(m_sceneDesc.meshFilenames.size());
    for (auto&& [idx, meshName] : std::views::enumerate(m_sceneDesc.meshFilenames)) {
        const std::filesystem::path relativePath = std::filesystem::path("Models") / meshName;
        auto mesh{loadTriangleMesh(renderer->getResourcesPath() / relativePath).unwrap()};
        mesh.transform(m_sceneDesc.transforms[idx]);

        auto& geometry =
            m_resourceContext->addGeometry(fmt::format("shape-{}", idx), createRayTracingGeometry(*m_renderer, mesh));

        auto& props = m_sceneDesc.props[idx];
        props.positions = geometry.getVertexBuffer(0)->getDeviceAddress();
        props.attributes = geometry.getVertexBuffer(1)->getDeviceAddress();
        props.triangles = geometry.getIndexBuffer()->getDeviceAddress();

        if (props.lightId != -1) {
            props.aliasTable =
                m_resourceContext
                    ->addBuffer(
                        fmt::format("aliasTable-{}", idx), createAliasTableBuffer(*m_renderer, createAliasTable(mesh)))
                    ->getDeviceAddress();
        }

        instances.push_back({
            .geometry = &geometry,
            .triangleCount = mesh.getTriangleCount(),
            .customIndex = static_cast<uint32_t>(idx),
        });
    }

    m_instancePropsBuffer = m_resourceContext->createStorageBuffer(
        "instanceProps", m_sceneDesc.props, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

    m_sceneAddresses = {
        .instances = m_instancePropsBuffer->getDeviceAddress(),
        .materials = m_bsdfParamsBuffer->getDeviceAddress(),
        .lights = m_lightParamsBuffer->getDeviceAddress(),
        .environmentCdf = m_environmentCdfBuffer ? m_environmentCdfBuffer->getDeviceAddress() : 0,
    };

    m_pathTracer = std::make_unique<PathTracer>(
        *m_renderer,
        PathTracerCreateInfo{
            .debugName = "Path Tracer",
            .shaderStages = kShaderStages,
            .resourceHeapSlotCount = kPathTracerMaterialTextureFirstSlot + static_cast<uint32_t>(m_materialImages.size()),
            .samplerHeapSlotCount = kPathTracerSamplerHeapSlotCount,
            .integratorParamsSize = sizeof(IntegratorParameters),
        },
        instances);

    auto& samplerHeap = m_pathTracer->getSamplerHeap();
    samplerHeap.write(kPathTracerEnvironmentSamplerSlot, createLatLongEnvironmentSamplerCreateInfo());
    samplerHeap.write(kPathTracerMaterialSamplerSlot, createLinearRepeatSamplerCreateInfo());

    // The kBsdfOpenPbr branch reads this unconditionally: directional-albedo.part.glsl refuses to compile
    // without the sampler, rather than silently degrading every compensation mode to a no-op.
    m_ggxAlbedoLut = bindGgxAlbedoLut(*m_renderer, *m_pathTracer);

    buildRenderGraph();
}

void VulkanRayTracingScene::buildRenderGraph() {
    m_renderGraph = std::make_unique<rg::RenderGraph>();
    m_renderGraph->addPass(
        "path-trace",
        PassType::RayTracing,
        [this](rg::RenderGraph::Builder& builder) {
            builder.getBlackboard().insert<PathTracingPassData>().image = builder.createStorageImage(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = static_cast<uint32_t>(m_renderResolution.x),
                    .height = static_cast<uint32_t>(m_renderResolution.y),
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
    static_cast<void>(width);
    static_cast<void>(height);

    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    updateDescriptorHeap();
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&PathTracingPassData::image>());
    m_pathTracer->resetAccumulation();
}

void VulkanRayTracingScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    m_pathTracer->updateCamera(m_cameraController->getCameraParameters());
}

void VulkanRayTracingScene::render(const FrameContext& frameContext) {
    CRISP_TRACE_VK_SCOPE("VulkanRayTracingScene::render", frameContext.commandEncoder);

    const int32_t accumulatedSamples = m_pathTracer->getAccumulatedSampleCount();
    m_integratorParams.frameIdx = m_pathTracer->getFrameIndex();
    m_integratorParams.sampleOffset = accumulatedSamples;
    m_integratorParams.sampleCount = m_samplesPerFrame;
    if (m_captureAfterSamples > 0) {
        m_integratorParams.sampleCount = std::min(m_samplesPerFrame, m_captureAfterSamples - accumulatedSamples);
        if (accumulatedSamples + m_integratorParams.sampleCount >= m_captureAfterSamples) {
            // traceRays records the readback after this dispatch, so arm it before executing the pass.
            m_screenshotRequested = true;
        }
    }

    m_pathTracer->uploadFrameData(frameContext, structAsBytes(m_integratorParams));

    frameContext.commandEncoder.insertBarrier(kRayTracingRead >> kTransferWrite);
    frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_bsdfParamsBuffer, 0, m_sceneDesc.bsdfs);
    if (!m_sceneDesc.lights.empty()) {
        frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_lightParamsBuffer, 0, m_sceneDesc.lights);
    }
    frameContext.commandEncoder.insertBarrier(kTransferWrite >> kRayTracingRead);

    m_renderGraph->execute(frameContext);

    m_pathTracer->advance(m_integratorParams.sampleCount);

    if (m_captureAfterSamples > 0 && m_pathTracer->getAccumulatedSampleCount() >= m_captureAfterSamples) {
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
    const auto extent = m_renderGraph->getImageExtent(m_renderGraph->getBlackboard().get<PathTracingPassData>().image);
    m_pathTracer->trace(frameContext, {extent.width, extent.height}, structAsBytes(m_sceneAddresses));

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
    ImGui::LabelText("Acc. Samples", "%d", m_pathTracer->getAccumulatedSampleCount()); // NOLINT
    if (ImGui::InputInt("Max Bounces", &m_integratorParams.maxBounces)) {
        m_pathTracer->resetAccumulation();
    }
    if (ImGui::InputInt("Samples per Frame", &m_samplesPerFrame)) {
        m_samplesPerFrame = std::max(1, m_samplesPerFrame);
        m_pathTracer->resetAccumulation();
    }

    ImGui::Separator();

    if (!m_sceneDesc.lights.empty()) {
        const bool isPointLight = m_sceneDesc.lights[0].type == kLightPoint;
        const bool isDirectionalLight = m_sceneDesc.lights[0].type == kLightDirectional;
        const char* redLabel =
            isPointLight ? "Point Power R" : (isDirectionalLight ? "Directional Irradiance R" : "Area Radiance R");
        const char* greenLabel =
            isPointLight ? "Point Power G" : (isDirectionalLight ? "Directional Irradiance G" : "Area Radiance G");
        const char* blueLabel =
            isPointLight ? "Point Power B" : (isDirectionalLight ? "Directional Irradiance B" : "Area Radiance B");
        if (ImGui::SliderFloat(redLabel, &m_sceneDesc.lights[0].emission[0], 0.0f, 50.0f)) {
            m_pathTracer->resetAccumulation();
        }
        if (ImGui::SliderFloat(greenLabel, &m_sceneDesc.lights[0].emission[1], 0.0f, 50.0f)) {
            m_pathTracer->resetAccumulation();
        }
        if (ImGui::SliderFloat(blueLabel, &m_sceneDesc.lights[0].emission[2], 0.0f, 50.0f)) {
            m_pathTracer->resetAccumulation();
        }
    }
    if (m_sceneDesc.bsdfs.size() > 5 &&
        ImGui::SliderFloat("Int IOR", &m_sceneDesc.bsdfs[5].surface.specularIor, 1.0f, 10.0f)) {
        m_pathTracer->resetAccumulation();
    }

    ImGui::Separator();

    if (ImGui::RadioButton("MIS Path Tracing", m_integratorParams.samplingMode == 0)) {
        m_integratorParams.samplingMode = 0;
        m_pathTracer->resetAccumulation();
    }
    if (ImGui::RadioButton("Pure Path Tracing", m_integratorParams.samplingMode == 1)) {
        m_integratorParams.samplingMode = 1;
        m_pathTracer->resetAccumulation();
    }
    if (ImGui::RadioButton("Light-Sampled Direct", m_integratorParams.samplingMode == 2)) {
        m_integratorParams.samplingMode = 2;
        m_pathTracer->resetAccumulation();
    }
    if (ImGui::RadioButton("Direct MIS", m_integratorParams.samplingMode == 3)) {
        m_integratorParams.samplingMode = 3;
        m_pathTracer->resetAccumulation();
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

void VulkanRayTracingScene::updateDescriptorHeap() {
    m_pathTracer->setStorageImage(m_renderGraph->getImageView<&PathTracingPassData::image>());

    auto& resourceHeap = m_pathTracer->getResourceHeap();
    if (m_environmentImage) {
        resourceHeap.writeSampledImage(
            kPathTracerEnvironmentSlot, m_environmentImage->getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    for (uint32_t i = 0; i < m_materialImages.size(); ++i) {
        resourceHeap.writeSampledImage(
            kPathTracerMaterialTextureFirstSlot + i,
            m_materialImages[i]->getView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void VulkanRayTracingScene::setupInput() {
    m_window->keyPressed += [this](Key key, int) {
        switch (key) {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            m_pathTracer->resetAccumulation();
            break;
        default: {
        }
        }
    };
}

} // namespace crisp
