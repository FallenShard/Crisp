
#include <Crisp/Scenes/OceanScene.hpp>

#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphIo.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>
#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

namespace crisp {
namespace {
constexpr int32_t N = 256;
constexpr int32_t logN = std::bit_width(static_cast<uint32_t>(N)) - 1;
constexpr float kGravity = 9.81f;

// Non-power-of-two ratios on purpose: prevent visible tiling. The sizes are in meters.
constexpr std::array<float, kOceanCascadeCount> kPatchWorldSizes{503.0f, 127.0f, 31.0f};

// The mesh spans the coarsest cascade; the finer two tile inside it at their own periods.
constexpr float kMeshPatchWorldSize = kPatchWorldSizes[0];
constexpr int32_t kMeshTessellation = 512;

// The Phillips tail cutoff wants to sit just below the finest wave the cascades can represent.
constexpr float kFinestCellSize = kPatchWorldSizes[kOceanCascadeCount - 1] / N;

constexpr int32_t kMaxInstancesPerSide = 8;

constexpr int32_t kFoamGridSize = 512;
constexpr uint32_t kFoamLayerCount = 2;

constexpr uint32_t kFoamNoiseSize = 256;
constexpr uint32_t kFoamNoiseOctaves = 5;

// Mirrors the push constant block in Shaders/Common/ocean-draw.part.glsl.
struct OceanPushConstants {
    glm::vec3 sunDirection;
    float sunIntensity;

    float patchWorldSize;
    int32_t instancesPerSide;
    int32_t gridSize;
    float choppiness;

    float waterRoughness;
    float foamThreshold;
    float foamSoftness;
    float foamIntensity;
    float invRmsWaveHeight;
    float slopeVarianceScale;
    float foamPatchWorldSize;
    int32_t foamLayer;

    glm::vec4 cascadeSizes;
    glm::vec4 cascadeWavelengths;
    glm::vec4 cascadeSlopeVariances;

    float foamErosion;
    float foamFreshness;
};

static_assert(sizeof(OceanPushConstants) == 120);

struct OscillationPassData {
    RenderGraphResourceHandle packedDisplacement;
    RenderGraphResourceHandle packedJacobian;
};

template <size_t Tag>
struct HorizontalFftPassData {
    RenderGraphResourceHandle image;
};

template <size_t Tag>
struct VerticalFftPassData {
    RenderGraphResourceHandle image;
};

struct FoamPassData {
    RenderGraphResourceHandle foam;
};

struct OceanOutputData {
    RenderGraphResourceHandle hdrImage;
};

// Mirrors the push constant block in ocean-foam.comp.glsl.
struct FoamPushConstants {
    int32_t foamGridSize;
    float foamPatchWorldSize;
    float vertexSpacing;
    float choppiness;

    float deltaTime;
    float halfLife;
    float injectionThreshold;
    float injectionGain;

    glm::vec2 driftVelocity;
    int32_t readLayer;
    int32_t writeLayer;

    glm::vec4 cascadeSizes;
    glm::vec4 cascadeWavelengths;
};

static_assert(sizeof(FoamPushConstants) == 80);

// Y-up: elevation is measured from the horizon, azimuth around +Y from +Z.
glm::vec3 computeSunDirection(const float azimuthDegrees, const float elevationDegrees) {
    const float azimuth = glm::radians(azimuthDegrees);
    const float elevation = glm::radians(elevationDegrees);
    return {std::cos(elevation) * std::sin(azimuth), std::sin(elevation), std::cos(elevation) * std::cos(azimuth)};
}

struct ComputeDispatch {
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<Material> material;

    VkExtent3D workGroupSize;
    VkExtent3D dispatchSize;

    void bind(const FrameContext& ctx) const {
        ctx.commandEncoder.bindPipeline(*pipeline);
        ctx.commandEncoder.bindDescriptorSets(material->getDescriptorSetBinding());
    }
};

} // namespace

struct OceanPassResources {
    ComputeDispatch oscillation;
    ComputeDispatch foam;
    FlatStringHashMap<ComputeDispatch> ifft;
};

namespace {

ComputeDispatch createOscillationPassDispatch(Renderer& renderer, const ImageCache& imageCache) {
    ComputeDispatch dispatch{};
    dispatch.workGroupSize = {16, 16, 1};
    dispatch.dispatchSize = computeWorkGroupCount(glm::uvec3(N, N, 1), dispatch.workGroupSize);
    dispatch.pipeline = createComputePipeline(
        renderer.getDevice(), renderer.getAssetPaths().getShaderSpvPath("ocean-spectrum.comp"), dispatch.workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    dispatch.material->writeDescriptor(
        0, 0, imageCache.getImageView("randImageView").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    return dispatch;
}

constexpr uint32_t kMaxNConstantId = 3;
constexpr uint32_t kApplyOriginShiftConstantId = 4;
constexpr uint32_t kTransposedConstantId = 5;

template <bool Horizontal>
ComputeDispatch createIfftDispatch(Renderer& renderer) {
    ComputeDispatch dispatch{};
    // N/2 threads butterfly a whole line, one workgroup per line, one dispatch slice per cascade.
    dispatch.workGroupSize = {static_cast<uint32_t>(N / 2), 1, 1};
    dispatch.dispatchSize = {1, static_cast<uint32_t>(N), kOceanCascadeCount};

    SpecializationConstantMap specializationConstants{{kMaxNConstantId, static_cast<uint32_t>(N)}};
    if constexpr (!Horizontal) {
        specializationConstants.emplace(kApplyOriginShiftConstantId, VK_TRUE);
        specializationConstants.emplace(kTransposedConstantId, VK_TRUE);
    }
    dispatch.pipeline = createComputePipeline(
        renderer.getDevice(),
        renderer.getAssetPaths().getShaderSpvPath("ifft.comp"),
        dispatch.workGroupSize,
        {},
        specializationConstants);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    return dispatch;
}

template <size_t Tag>
void createFftDispatches(OceanPassResources& passResources, Renderer& renderer) {
    passResources.ifft[fmt::format("ifft-h-{}", Tag)] = createIfftDispatch<true>(renderer);
    passResources.ifft[fmt::format("ifft-v-{}", Tag)] = createIfftDispatch<false>(renderer);
}

template <size_t Tag>
void writeFftDispatchDescriptors(
    OceanPassResources& passResources, const rg::RenderGraph& renderGraph, const VulkanImageView& srcView) {
    const auto& horiView =
        renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image);
    const auto& vertView =
        renderGraph.getResourceImageView(renderGraph.getBlackboard().get<VerticalFftPassData<Tag>>().image);

    auto& hori = passResources.ifft.at(fmt::format("ifft-h-{}", Tag));
    hori.material->writeDescriptor(0, 0, srcView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    hori.material->writeDescriptor(0, 1, horiView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

    auto& vert = passResources.ifft.at(fmt::format("ifft-v-{}", Tag));
    vert.material->writeDescriptor(0, 0, horiView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    vert.material->writeDescriptor(0, 1, vertView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
}

} // namespace

OceanScene::OceanScene(Renderer* renderer, Window* window)
    : Scene(renderer, window)
    , m_oceanParams(createOceanParameters(N, 10.0f, 0.0f, 0.001f, kFinestCellSize))
    , m_cascades(createOceanCascades(kPatchWorldSizes, N))
    , m_choppiness(1.0f) {
    setupInput();
    setupResources();
    buildRenderGraph();

    m_renderer->getDevice().flushDescriptorUpdates();
}

void OceanScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](Key key, int /*modifiers*/) {
        if (key == Key::Space) {
            m_paused = !m_paused;
        } else if (key == Key::F5) {
            m_resourceContext->recreatePipelines();
        }
    }));
}

void OceanScene::setupResources() {
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformRingBuffer("camera", sizeof(CameraParameters));
    m_resourceContext->createUniformRingBuffer<TonemapParameters>(kTonemapBufferId);

    std::vector<std::vector<VertexAttributeDescriptor>> vertexFormat = {{VertexAttribute::Position}};
    TriangleMesh mesh = createGridMesh(kMeshPatchWorldSize, kMeshTessellation);
    m_resourceContext->addGeometry("ocean", createGeometry(*m_renderer, mesh, vertexFormat))
        .setInstanceCount(m_instancesPerSide * m_instancesPerSide);

    auto foamNoiseImage = createFoamNoise();
    m_resourceContext->imageCache.addImageView(
        "foamNoiseView", createView(m_renderer->getDevice(), *foamNoiseImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    m_resourceContext->imageCache.addImage("foamNoise", std::move(foamNoiseImage));

    auto spectrumImage = createInitialSpectrum();
    m_resourceContext->imageCache.addImageView(
        "randImageView",
        createView(m_renderer->getDevice(), *spectrumImage, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, kOceanCascadeCount));
    m_resourceContext->imageCache.addImage("randImage", std::move(spectrumImage));

    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), 16.0f));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), 16.0f));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), 16.0f, 9.0f));
    imageCache.addImage("brdfLut", integrateBrdfLut(m_renderer));

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 1);
    m_transformHandle = m_transformBuffer->getNextIndex();

    m_envLight = std::make_unique<EnvironmentLight>(
        *m_renderer,
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps/TableMountain").unwrap());

    resetCamera();
}

void OceanScene::resetCamera() {
    constexpr float kAngularSpeed = glm::radians(90.0f);

    const float patchOnScreenSize = kMeshPatchWorldSize * m_modelScale;
    const glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    const float distance = 1.6f * patchOnScreenSize;
    const float yaw = glm::radians(45.0f);
    const float pitch = -glm::atan(1.0f / glm::sqrt(2.0f));

    m_cameraController->setPosition(distance * direction);
    m_cameraController->updateOrientation(yaw / kAngularSpeed, pitch / kAngularSpeed);
    m_cameraController->setSpeed(std::max(0.05f, patchOnScreenSize * 0.15f));
}

OceanScene::~OceanScene() {
    m_passResources.reset();
}

void OceanScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);
    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    writeGraphDependentDescriptors();
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&TonemapPassData::image>());
}

void OceanScene::update(const UpdateParams& updateParams) {
    if (m_spectrumDirty) {
        // Integrated once at unit amplitude. Everything else in the spectrum -- wind, fetch, the
        // small-wave cutoff -- changes the shape and has to come back through here.
        OceanParameters unitAmplitudeParams{m_oceanParams};
        unitAmplitudeParams.A = 1.0f;
        for (uint32_t i = 0; i < kOceanCascadeCount; ++i) {
            m_unitAmplitudeMoments[i] = computeCascadeMoments(unitAmplitudeParams, m_cascades[i]);
        }
        m_spectrumDirty = false;
    }

    float heightVariance = 0.0f;
    for (uint32_t i = 0; i < kOceanCascadeCount; ++i) {
        m_cascadeMoments[i] = {
            .heightVariance = m_unitAmplitudeMoments[i].heightVariance * m_oceanParams.A,
            .slopeVariance = m_unitAmplitudeMoments[i].slopeVariance * m_oceanParams.A,
        };
        heightVariance += m_cascadeMoments[i].heightVariance;
    }
    m_rmsWaveHeight = std::sqrt(heightVariance);

    m_cameraController->update(updateParams.dt);
    const auto& cameraParams = m_cameraController->getCameraParameters();

    m_transformBuffer->getPack(m_transformHandle).M = glm::scale(glm::mat4(1.0f), glm::vec3(m_modelScale));
    m_transformBuffer->update(cameraParams.V, cameraParams.P);
    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(
        cameraParams, updateParams.frameInFlightIdx);
    m_resourceContext->getRingBuffer(kTonemapBufferId)
        ->updateStagingBufferFromStruct(m_tonemapParams, updateParams.frameInFlightIdx);
    m_transformBuffer->updateStagingBuffer(updateParams.frameInFlightIdx);
    m_skybox->updateTransforms(cameraParams.V, cameraParams.P, updateParams.frameInFlightIdx);

    // The foam pass integrates in seconds, so a paused sim must not keep decaying it.
    m_foamDeltaTime = m_paused ? 0.0f : updateParams.dt;
    if (!m_paused) {
        m_oceanParams.time += updateParams.dt;
    }
}

void OceanScene::render(const FrameContext& frameContext) {
    auto* cameraBuffer = m_resourceContext->getRingBuffer("camera");
    cameraBuffer->updateDeviceBuffer(frameContext.commandEncoder);
    m_resourceContext->getRingBuffer(kTonemapBufferId)->updateDeviceBuffer(frameContext.commandEncoder);

    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder);
    m_skybox->updateDeviceBuffer(frameContext.commandEncoder);
    frameContext.commandEncoder.insertBarrier(kTransferWrite >> (kVertexUniformRead | kFragmentUniformRead));

    m_renderGraph->execute(frameContext);
}

void OceanScene::drawGui() {
    ImGui::Begin("Ocean Parameters");
    float windDirectionDegrees = glm::degrees(std::atan2(m_oceanParams.windDirection.y, m_oceanParams.windDirection.x));
    if (ImGui::SliderFloat("Wind Direction", &windDirectionDegrees, -180.0f, 180.0f, "%.1f deg")) {
        const float radians = glm::radians(windDirectionDegrees);
        m_oceanParams.windDirection = {std::cos(radians), std::sin(radians)};
        m_spectrumDirty = true;
    }
    if (ImGui::SliderFloat("Wind Speed", &m_oceanParams.windSpeed, 0.1f, 40.0f, "%.1f m/s")) {
        m_oceanParams.Lw = m_oceanParams.windSpeed * m_oceanParams.windSpeed / kGravity;
        m_spectrumDirty = true;
    }
    int32_t spectrumModel = static_cast<int32_t>(m_oceanParams.spectrumModel);
    if (ImGui::Combo(
            "Spectrum",
            &spectrumModel,
            kOceanSpectrumModelNames.data(),
            static_cast<int32_t>(kOceanSpectrumModelNames.size()))) {
        m_oceanParams.spectrumModel = static_cast<OceanSpectrumModel>(spectrumModel);
        // The models disagree by orders of magnitude about what a physical amplitude is, so reset the
        // gain to something usable rather than leaving the sea flat or exploded after a swap.
        m_oceanParams.A = kOceanSpectrumDefaultAmplitudes[static_cast<size_t>(spectrumModel)];
        m_spectrumDirty = true;
    }
    if (m_oceanParams.spectrumModel != OceanSpectrumModel::Phillips) {
        if (ImGui::SliderFloat(
                "Fetch", &m_oceanParams.fetch, 1000.0f, 1000000.0f, "%.0f m", ImGuiSliderFlags_Logarithmic)) {
            m_spectrumDirty = true;
        }
        if (ImGui::SliderFloat("Directional Spread", &m_oceanParams.directionalSpread, 0.5f, 16.0f)) {
            m_spectrumDirty = true;
        }
    }
    if (m_oceanParams.spectrumModel == OceanSpectrumModel::Jonswap) {
        if (ImGui::SliderFloat("Peak Enhancement", &m_oceanParams.peakEnhancement, 1.0f, 7.0f)) {
            m_spectrumDirty = true;
        }
    }
    // Deliberately does not dirty the spectrum: the moments are linear in A, so update() rescales
    // them instead of re-integrating 3 * N^2 samples on the main thread for every slider pixel.
    ImGui::SliderFloat("Amplitude", &m_oceanParams.A, 1e-4f, 10.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
    if (ImGui::SliderFloat("Small Waves", &m_oceanParams.smallWaves, 0.0f, 4.0f * kFinestCellSize)) {
        m_spectrumDirty = true;
    }
    ImGui::TextDisabled("RMS wave height: %.2f m", m_rmsWaveHeight); // NOLINT
    for (uint32_t i = 0; i < kOceanCascadeCount; ++i) {
        ImGui::TextDisabled( // NOLINT
            "Cascade %u: %.0f m patch, %.2f-%.2f m waves, slope var %.4f",
            i,
            m_cascades[i].patchWorldSize,
            2.0f * m_cascades[i].patchWorldSize / static_cast<float>(N),
            m_cascades[i].kMin > 0.0f ? 2.0f * glm::pi<float>() / m_cascades[i].kMin
                                      : m_cascades[i].patchWorldSize,
            m_cascadeMoments[i].slopeVariance);
    }
    ImGui::SliderFloat("Choppiness", &m_choppiness, 0.0f, 2.0f);
    ImGui::SliderFloat("Water Roughness", &m_waterRoughness, 0.03f, 0.35f);
    ImGui::SliderFloat("Specular AA Strength", &m_slopeVarianceScale, 0.0f, 4.0f);
    ImGui::SliderFloat("Foam Half-Life", &m_foamHalfLife, 0.05f, 20.0f, "%.2f s", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Foam Injection Threshold", &m_foamInjectionThreshold, 0.0f, 2.0f);
    ImGui::SliderFloat("Foam Injection Gain", &m_foamInjectionGain, 0.0f, 8.0f);
    ImGui::SliderFloat("Foam Drift Speed", &m_foamDriftSpeed, 0.0f, 5.0f, "%.2f m/s");
    ImGui::SliderFloat("Foam Erosion", &m_foamErosion, 0.0f, 2.0f);
    ImGui::SliderFloat("Foam Freshness", &m_foamFreshness, 0.05f, 4.0f);
    ImGui::SliderFloat("Foam Threshold", &m_foamThreshold, 0.0f, 2.0f);
    ImGui::SliderFloat("Foam Softness", &m_foamSoftness, 0.01f, 1.0f);
    ImGui::SliderFloat("Foam Intensity", &m_foamIntensity, 0.0f, 2.0f);
    ImGui::SliderFloat("Sun Azimuth", &m_sunAzimuthDegrees, -180.0f, 180.0f, "%.1f deg");
    ImGui::SliderFloat("Sun Elevation", &m_sunElevationDegrees, 0.0f, 90.0f, "%.1f deg");
    ImGui::SliderFloat("Sun Intensity", &m_sunIntensity, 0.0f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::Combo(
        "Tonemap Operator",
        &m_tonemapParams.operatorIndex,
        kTonemapOperatorNames.data(),
        static_cast<int32_t>(kTonemapOperatorNames.size()));
    ImGui::SliderFloat("Exposure", &m_tonemapParams.exposure, 0.05f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Model Scale", &m_modelScale, 0.001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
    if (ImGui::SliderInt("Instances Per Side", &m_instancesPerSide, 1, kMaxInstancesPerSide)) {
        m_resourceContext->getGeometry("ocean").setInstanceCount(m_instancesPerSide * m_instancesPerSide);
    }
    if (ImGui::Button("Reset View")) {
        resetCamera();
    }
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(440.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Render Graph")) {
        drawRenderGraphGui(*m_renderGraph);
    }
    ImGui::End();
}

std::unique_ptr<VulkanImage> OceanScene::createInitialSpectrum() {
    const auto oceanSpectrum{createOceanSpectrum(0, m_oceanParams, kOceanCascadeCount)};

    auto image = createStorageImage(m_renderer->getDevice(), kOceanCascadeCount, N, N, VK_FORMAT_R32G32_SFLOAT);
    const auto staging = createStagingBuffer(
        m_renderer->getDevice(), oceanSpectrum.data(), oceanSpectrum.size() * sizeof(oceanSpectrum[0]));
    m_renderer->getDevice().getGeneralQueue().submitAndWait([&staging, img = image.get()](VkCommandBuffer cmdBuffer) {
        const VulkanCommandEncoder encoder(cmdBuffer);
        encoder.transitionLayout(*img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite);
        const VkBufferImageCopy region{
            .bufferRowLength = img->getWidth(),
            .bufferImageHeight = img->getHeight(),
            .imageSubresource =
                {
                    .aspectMask = img->getAspectMask(),
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = kOceanCascadeCount,
                },
            .imageExtent = {img->getWidth(), img->getHeight(), 1},
        };
        encoder.copyBufferToImage(*staging, *img, region);
        encoder.transitionLayout(*img, VK_IMAGE_LAYOUT_GENERAL, kTransferWrite >> kComputeStorageWrite);
    });

    return image;
}

std::unique_ptr<VulkanImage> OceanScene::createFoamNoise() {
    const auto noise{createTileableFoamNoise(kFoamNoiseSize, /*seed=*/11, kFoamNoiseOctaves)};

    auto image = createStorageImage(m_renderer->getDevice(), 1, kFoamNoiseSize, kFoamNoiseSize, VK_FORMAT_R32_SFLOAT);
    const auto staging = createStagingBuffer(m_renderer->getDevice(), noise.data(), noise.size() * sizeof(noise[0]));
    m_renderer->getDevice().getGeneralQueue().submitAndWait([&staging, img = image.get()](VkCommandBuffer cmdBuffer) {
        const VulkanCommandEncoder encoder(cmdBuffer);
        encoder.transitionLayout(*img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite);
        const VkBufferImageCopy region{
            .bufferRowLength = img->getWidth(),
            .bufferImageHeight = img->getHeight(),
            .imageSubresource =
                {
                    .aspectMask = img->getAspectMask(),
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageExtent = {img->getWidth(), img->getHeight(), 1},
        };
        encoder.copyBufferToImage(*staging, *img, region);
        // Only ever sampled, so it can settle in its read layout here and never move again.
        encoder.transitionLayout(*img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kTransferWrite >> kFragmentSampledRead);
    });

    return image;
}

void OceanScene::buildRenderGraph() {
    m_passResources = std::make_unique<OceanPassResources>();
    m_renderGraph = std::make_unique<rg::RenderGraph>();
    m_renderGraph->getBlackboard().insert<OscillationPassData>();
    m_renderGraph->addPass(
        "oscillation",
        PassType::Compute,
        [](rg::RenderGraph::Builder& builder) {
            constexpr RenderGraphImageDescription kCascadeImage{
                .sizePolicy = SizePolicy::Absolute,
                .width = N,
                .height = N,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .layerCount = kOceanCascadeCount,
            };

            auto& data = builder.getBlackboard().get<OscillationPassData>();
            data.packedDisplacement =
                builder.createStorageImage(kCascadeImage, fmt::format("{}-packed-displacement", "oscillation"));
            data.packedJacobian =
                builder.createStorageImage(kCascadeImage, fmt::format("{}-packed-jacobian", "oscillation"));
        },
        [this](const FrameContext& ctx) {
            m_passResources->oscillation.bind(ctx);
            for (uint32_t i = 0; i < kOceanCascadeCount; ++i) {
                ctx.commandEncoder.setPushConstants(
                    *m_passResources->oscillation.pipeline->getPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT,
                    createOceanSpectrumPushConstants(m_oceanParams, m_cascades[i], static_cast<int32_t>(i)));
                ctx.commandEncoder.dispatchCompute(m_passResources->oscillation.dispatchSize);
            }
        });

    auto addFftPasses = [this]<size_t Tag>(const RenderGraphResourceHandle image) {
        struct IfftPushConstants {
            int32_t N;
            int32_t logN;
        };

        const std::string horiPassName{fmt::format("ifft-h-{}", Tag)};
        m_renderGraph->addPass(
            horiPassName,
            PassType::Compute,
            [image, horiPassName](rg::RenderGraph::Builder& builder) {
                builder.readStorageImage(image);
                auto& data = builder.getBlackboard().insert<HorizontalFftPassData<Tag>>();
                data.image = builder.createStorageImage(
                    {
                        .sizePolicy = SizePolicy::Absolute,
                        .width = N,
                        .height = N,
                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                        .layerCount = kOceanCascadeCount,
                    },
                    fmt::format("{}-image", horiPassName));
            },
            [this](const FrameContext& ctx) {
                const auto& dispatch{m_passResources->ifft.at(fmt::format("ifft-h-{}", Tag))};
                dispatch.bind(ctx);
                ctx.commandEncoder.setPushConstants(
                    *dispatch.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, IfftPushConstants{N, logN});
                ctx.commandEncoder.dispatchCompute(dispatch.dispatchSize);
            });

        const std::string vertPassName{fmt::format("ifft-v-{}", Tag)};
        m_renderGraph->addPass(
            vertPassName,
            PassType::Compute,
            [vertPassName](rg::RenderGraph::Builder& builder) {
                builder.readStorageImage(builder.getBlackboard().get<HorizontalFftPassData<Tag>>().image);
                auto& data = builder.getBlackboard().insert<VerticalFftPassData<Tag>>();
                data.image = builder.createStorageImage(
                    {.sizePolicy = SizePolicy::Absolute,
                     .width = N,
                     .height = N,
                     .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                     .layerCount = kOceanCascadeCount},
                    fmt::format("{}-image", vertPassName));
            },
            [this](const FrameContext& ctx) {
                const auto& dispatch{m_passResources->ifft.at(fmt::format("ifft-v-{}", Tag))};
                dispatch.bind(ctx);
                ctx.commandEncoder.setPushConstants(
                    *dispatch.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, IfftPushConstants{N, logN});
                ctx.commandEncoder.dispatchCompute(dispatch.dispatchSize);
            });
    };
    addFftPasses.operator()<0>(m_renderGraph->getBlackboard().get<OscillationPassData>().packedDisplacement);
    addFftPasses.operator()<1>(m_renderGraph->getBlackboard().get<OscillationPassData>().packedJacobian);

    m_renderGraph->addPass(
        "foam",
        PassType::Compute,
        [](rg::RenderGraph::Builder& builder) {
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<1>>().image);

            auto& data = builder.getBlackboard().insert<FoamPassData>();
            data.foam = builder.createStorageImage(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = kFoamGridSize,
                    .height = kFoamGridSize,
                    .format = VK_FORMAT_R32_SFLOAT,
                    .layerCount = kFoamLayerCount,
                },
                "foam-accumulation");
        },
        [this](const FrameContext& ctx) {
            glm::vec4 cascadeSizes{0.0f};
            glm::vec4 cascadeWavelengths{0.0f};
            for (uint32_t i = 0; i < kOceanCascadeCount; ++i) {
                cascadeSizes[static_cast<int32_t>(i)] = m_cascades[i].patchWorldSize;
                cascadeWavelengths[static_cast<int32_t>(i)] = computeBandWavelength(m_cascades[i]);
            }

            // Ping-pong by frame parity. Reading one layer while writing the other is what makes the
            // drift gather safe without a second image.
            const int32_t writeLayer = static_cast<int32_t>(ctx.frameIndex % kFoamLayerCount);

            m_passResources->foam.bind(ctx);
            ctx.commandEncoder.setPushConstants(
                *m_passResources->foam.pipeline->getPipelineLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT,
                FoamPushConstants{
                    .foamGridSize = kFoamGridSize,
                    .foamPatchWorldSize = kMeshPatchWorldSize,
                    .vertexSpacing = kMeshPatchWorldSize / kMeshTessellation,
                    .choppiness = m_choppiness,
                    .deltaTime = m_foamDeltaTime,
                    .halfLife = m_foamHalfLife,
                    .injectionThreshold = m_foamInjectionThreshold,
                    .injectionGain = m_foamInjectionGain,
                    .driftVelocity = m_oceanParams.windDirection * m_foamDriftSpeed,
                    .readLayer = 1 - writeLayer,
                    .writeLayer = writeLayer,
                    .cascadeSizes = cascadeSizes,
                    .cascadeWavelengths = cascadeWavelengths});
            ctx.commandEncoder.dispatchCompute(m_passResources->foam.dispatchSize);
        });

    m_renderGraph->addPass(
        kForwardLightingPass,
        PassType::Rasterizer,
        [](rg::RenderGraph::Builder& builder) {
            constexpr auto kOceanMapRead = kVertexSampledRead | kFragmentSampledRead;
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<0>>().image, kOceanMapRead);
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<1>>().image, kOceanMapRead);
            builder.readTexture(builder.getBlackboard().get<FoamPassData>().foam);
            auto& data = builder.getBlackboard().insert<OceanOutputData>();
            data.hdrImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-color", kForwardLightingPass),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 0.0f}}});
            builder.exportTexture(data.hdrImage);

            builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D32_SFLOAT,
                },
                fmt::format("{}-depth", kForwardLightingPass),
                VkClearValue{.depthStencil{0.0f, 0}});
        },
        [this](const FrameContext& ctx) {
            glm::vec4 cascadeSizes{0.0f};
            glm::vec4 cascadeWavelengths{0.0f};
            glm::vec4 cascadeSlopeVariances{0.0f};
            for (uint32_t i = 0; i < kOceanCascadeCount; ++i) {
                cascadeSizes[static_cast<int32_t>(i)] = m_cascades[i].patchWorldSize;
                cascadeWavelengths[static_cast<int32_t>(i)] = computeBandWavelength(m_cascades[i]);
                cascadeSlopeVariances[static_cast<int32_t>(i)] = m_cascadeMoments[i].slopeVariance;
            }

            auto& geometry = m_resourceContext->getGeometry("ocean");
            ctx.commandEncoder.bindPipeline(*m_oceanPipeline);
            ctx.commandEncoder.setViewport(m_renderer->getDefaultViewport());
            ctx.commandEncoder.setScissor(m_renderer->getDefaultScissor());
            ctx.commandEncoder.setPushConstants(
                *m_oceanPipeline->getPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                OceanPushConstants{
                    .sunDirection = computeSunDirection(m_sunAzimuthDegrees, m_sunElevationDegrees),
                    .sunIntensity = m_sunIntensity,
                    .patchWorldSize = kMeshPatchWorldSize,
                    .instancesPerSide = m_instancesPerSide,
                    .gridSize = kMeshTessellation,
                    .choppiness = m_choppiness,
                    .waterRoughness = m_waterRoughness,
                    .foamThreshold = m_foamThreshold,
                    .foamSoftness = m_foamSoftness,
                    .foamIntensity = m_foamIntensity,
                    .invRmsWaveHeight = 1.0f / std::max(m_rmsWaveHeight, 1e-4f),
                    .slopeVarianceScale = m_slopeVarianceScale,
                    .foamPatchWorldSize = kMeshPatchWorldSize,
                    .foamLayer = static_cast<int32_t>(ctx.frameIndex % kFoamLayerCount),
                    .cascadeSizes = cascadeSizes,
                    .cascadeWavelengths = cascadeWavelengths,
                    .cascadeSlopeVariances = cascadeSlopeVariances,
                    .foamErosion = m_foamErosion,
                    .foamFreshness = m_foamFreshness});
            ctx.commandEncoder.bindDescriptorSets(m_oceanMaterial->getDescriptorSetBinding());
            geometry.bindAndDraw(ctx.commandEncoder);

            const RenderNode& skyboxNode = m_skybox->getRenderNode();
            const auto& skyboxMaterialData = skyboxNode.materials.at({kForwardLightingPass, 0}).at(-1);
            ctx.commandEncoder.bindPipeline(*skyboxMaterialData.material->getPipeline());
            ctx.commandEncoder.bindDescriptorSets(skyboxMaterialData.material->getDescriptorSetBinding());
            skyboxNode.geometry->bindAndDraw(ctx.commandEncoder);
        });

    addTonemapPass(
        *m_renderGraph,
        *m_renderer,
        *m_resourceContext,
        m_renderGraph->getBlackboard().get<OceanOutputData>().hdrImage,
        // The render test copies the tonemapped image straight out; see Scenes/Test.
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&TonemapPassData::image>());

    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass),
        m_envLight->getCubeMapView(),
        m_resourceContext->imageCache.getSampler("linearClamp"));

    m_passResources->oscillation = createOscillationPassDispatch(*m_renderer, m_resourceContext->imageCache);

    auto& foamDispatch = m_passResources->foam;
    foamDispatch.workGroupSize = {16, 16, 1};
    foamDispatch.dispatchSize =
        computeWorkGroupCount(glm::uvec3(kFoamGridSize, kFoamGridSize, 1), foamDispatch.workGroupSize);
    foamDispatch.pipeline = createComputePipeline(
        m_renderer->getDevice(),
        m_renderer->getAssetPaths().getShaderSpvPath("ocean-foam.comp"),
        foamDispatch.workGroupSize);
    foamDispatch.material = std::make_unique<Material>(foamDispatch.pipeline.get());
    createFftDispatches<0>(*m_passResources, *m_renderer);
    createFftDispatches<1>(*m_passResources, *m_renderer);

    m_oceanPipeline = m_resourceContext->createPipeline(
        "ocean", "Ocean.json", m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass));
    m_oceanMaterial = m_resourceContext->createMaterial("ocean", m_oceanPipeline);
    m_oceanMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    m_oceanMaterial->writeDescriptor(1, 0, *m_resourceContext->getRingBuffer("camera"));
    m_oceanMaterial->writeDescriptor(
        1, 1, m_envLight->getDiffuseMapView(), m_resourceContext->imageCache.getSampler("linearClamp"));
    m_oceanMaterial->writeDescriptor(
        1, 2, m_envLight->getSpecularMapView(), m_resourceContext->imageCache.getSampler("linearMipmap"));
    m_oceanMaterial->writeDescriptor(
        1,
        3,
        m_resourceContext->imageCache.getImage("brdfLut").getView(),
        m_resourceContext->imageCache.getSampler("linearClamp"));

    writeGraphDependentDescriptors();
}

void OceanScene::writeGraphDependentDescriptors() {
    const auto& oscillationData = m_renderGraph->getBlackboard().get<OscillationPassData>();
    const auto& seedView = [this](const RenderGraphResourceHandle handle) -> const VulkanImageView& {
        return m_renderGraph->getResourceImageView(handle);
    };

    auto& oscillation = *m_passResources->oscillation.material;
    oscillation.writeDescriptor(
        0, 1, seedView(oscillationData.packedDisplacement).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    oscillation.writeDescriptor(
        0, 2, seedView(oscillationData.packedJacobian).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

    writeFftDispatchDescriptors<0>(*m_passResources, *m_renderGraph, seedView(oscillationData.packedDisplacement));
    writeFftDispatchDescriptors<1>(*m_passResources, *m_renderGraph, seedView(oscillationData.packedJacobian));

    const auto finalFftView = [this]<size_t Tag>() -> const VulkanImageView& {
        return m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<VerticalFftPassData<Tag>>().image);
    };
    auto& linearRepeat = m_resourceContext->imageCache.getSampler("linearRepeat");

    const auto& foamView = m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<FoamPassData>().foam);
    m_passResources->foam.material->writeDescriptor(0, 0, finalFftView.operator()<1>(), linearRepeat);
    m_passResources->foam.material->writeDescriptor(0, 1, foamView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

    m_oceanMaterial->writeDescriptor(0, 1, finalFftView.operator()<0>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 2, finalFftView.operator()<1>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 3, foamView, linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 4, m_resourceContext->imageCache.getImageView("foamNoiseView"), linearRepeat);

    m_renderer->getDevice().flushDescriptorUpdates();
}

} // namespace crisp
