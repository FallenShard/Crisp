
#include <Crisp/Scenes/OceanScene.hpp>

#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Lights/EnvironmentLight.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Models/Atmosphere.hpp>
#include <Crisp/Models/AtmosphereGui.hpp>
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

// The Phillips tail cutoff wants to sit just below the finest wave the cascades can represent.
constexpr float kFinestCellSize = kPatchWorldSizes[kOceanCascadeCount - 1] / N;

constexpr float kFoamWindowSize = 1024.0f;
constexpr int32_t kFoamGridSize = 1024;
constexpr float kFoamCellSize = kFoamWindowSize / static_cast<float>(kFoamGridSize);
constexpr uint32_t kFoamLayerCount = 2;

// Level 16 reaches 2000 km, well past the point where float world coordinates stop being exact.
constexpr int32_t kMaxClipmapLevels = 16;

constexpr uint32_t kTilesPerTaskGroupConstantId = 0;
constexpr uint32_t kTaskGroupsPerBlockConstantId = 1;
constexpr uint32_t kTileHeightSigmasConstantId = 2;

constexpr uint32_t kOceanTilesPerTaskGroup = 32;
constexpr uint32_t kOceanTaskGroupsPerBlock = 8;
constexpr float kOceanTileHeightSigmas = 4.0f;

constexpr VkQueryPipelineStatisticFlags kOceanGeometryStats =
    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;

constexpr const char* kAtmosphereBufferId = "atmosphereBuffer";

constexpr uint32_t kFoamNoiseSize = 256;
constexpr uint32_t kFoamNoiseOctaves = 5;

// Mirrors the push constant block in Shaders/Common/ocean-draw.part.glsl.
struct OceanPushConstants {
    glm::vec2 clipmapOrigin;
    float clipmapFinestSpacing;
    int32_t clipmapBlockQuads;

    float choppiness;
    float waterRoughness;
    float foamThreshold;
    float foamSoftness;

    float foamIntensity;
    float invRmsWaveHeight;
    float slopeVarianceScale;
    float foamWindowSize;

    int32_t foamLayer;
    float foamErosion;
    float foamFreshness;
    float planetRadius;

    glm::vec4 cascadeSizes;
    glm::vec4 cascadeWavelengths;
    glm::vec4 cascadeSlopeVariances;
};

static_assert(sizeof(OceanPushConstants) == 112);

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
    float foamWindowSize;
    float vertexSpacing;
    float choppiness;

    float deltaTime;
    float halfLife;
    float injectionThreshold;
    float injectionGain;

    glm::vec2 driftVelocity;
    int32_t readLayer;
    int32_t writeLayer;

    glm::vec2 anchor;
    glm::vec2 previousAnchor;

    glm::vec4 cascadeSizes;
    glm::vec4 cascadeWavelengths;
};

static_assert(sizeof(FoamPushConstants) == 96);

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

    m_pipelineStatsQueryPool = std::make_unique<VulkanPipelineStatsQueryPool>(
        m_renderer->getDevice(), kOceanGeometryStats, kRendererVirtualFrameCount, fmt::format("Ocean Geometry Stats"));
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
    m_resourceContext->createUniformRingBuffer<AtmosphereParameters>(kAtmosphereBufferId);

    std::vector<std::vector<VertexAttributeDescriptor>> vertexFormat = {{VertexAttribute::Position}};
    // One block template in cell units: unit spacing, centred on the origin. Every clipmap instance
    // is this same mesh placed and scaled by ocean.vert, so the whole ocean is a single draw.
    TriangleMesh mesh = createGridMesh(static_cast<float>(m_clipmap.blockQuads), m_clipmap.blockQuads);
    m_resourceContext->addGeometry("ocean", createGeometry(*m_renderer, mesh, vertexFormat))
        .setInstanceCount(static_cast<uint32_t>(computeClipmapInstanceCount(m_clipmap)));

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
    // addAtmosphereLutPasses registers its own "linearClamp"; ImageCache::addSampler replaces the
    // entry and destroys the old sampler, which the bindless registry is still pointing at, so this
    // one needs a key of its own.
    imageCache.addSampler("oceanLinearClamp", createLinearClampSampler(m_renderer->getDevice(), 16.0f));
    imageCache.addImage("brdfLut", loadBrdfLut(m_renderer));

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 1);
    m_transformHandle = m_transformBuffer->getNextIndex();

    resetCamera();
}

void OceanScene::resetCamera() {
    constexpr float kAngularSpeed = glm::radians(90.0f);

    // Level 0's half-extent, which is as far as the finest tessellation reaches.
    const float nearFieldSize = 2.0f * static_cast<float>(m_clipmap.blockQuads) * m_clipmap.finestSpacing;
    // Low and nearly level: the near field and the horizon are both in frame, which is what the
    // clipmap has to get right at once.
    const float yaw = glm::radians(45.0f);
    const float pitch = glm::radians(-10.0f);

    m_cameraController->setPosition(glm::vec3(0.0f, 0.8f * nearFieldSize, 0.0f));
    m_cameraController->updateOrientation(yaw / kAngularSpeed, pitch / kAngularSpeed);
    m_cameraController->setSpeed(std::max(0.05f, nearFieldSize * 0.15f));
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

    // The clipmap places itself in world metres, so any model transform would fight the snapping.
    m_transformBuffer->getPack(m_transformHandle).M = glm::mat4(1.0f);
    const glm::vec3 cameraPosition = m_cameraController->getCamera().getPosition();
    m_clipmapOrigin = computeClipmapOrigin(m_clipmap, glm::vec2(cameraPosition.x, cameraPosition.z));
    // Snapped to a whole texel: the toroidal addressing only keeps a texel's world identity if the
    // window moves in texel steps.
    m_foamAnchor = glm::floor(glm::vec2(cameraPosition.x, cameraPosition.z) / kFoamCellSize) * kFoamCellSize;
    m_transformBuffer->update(cameraParams.V, cameraParams.P);
    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(
        cameraParams, updateParams.frameInFlightIdx);
    m_resourceContext->getRingBuffer(kTonemapBufferId)
        ->updateStagingBufferFromStruct(m_tonemapParams, updateParams.frameInFlightIdx);

    // One sun drives the sky, the reflection and the specular highlight, which is item 4 closed by
    // construction rather than by dialling a constant onto a cubemap.
    applyAtmosphereSettings(m_atmosphereSettings, m_atmosphereParams);
    m_atmosphereParams.cameraPosition = cameraPosition / kMetersPerKilometer;
    m_atmosphereParams.VP = cameraParams.P * cameraParams.V;
    m_atmosphereParams.invVP = glm::inverse(m_atmosphereParams.VP);
    m_atmosphereParams.screenResolution = cameraParams.screenSize;
    m_resourceContext->getRingBuffer(kAtmosphereBufferId)
        ->updateStagingBufferFromStruct(m_atmosphereParams, updateParams.frameInFlightIdx);
    m_transformBuffer->updateStagingBuffer(updateParams.frameInFlightIdx);

    // The foam pass integrates in seconds, so a paused sim must not keep decaying it.
    m_foamDeltaTime = m_paused ? 0.0f : updateParams.dt;
    if (!m_paused) {
        m_oceanParams.time += updateParams.dt;
    }
}

void OceanScene::beginPipelineStatsFrame(const uint32_t virtualFrameIndex) {
    if (m_pipelineStatsQueryPool->isPending(virtualFrameIndex)) {
        if (!m_pipelineStatsQueryPool->tryGetResults(m_pipelineStats, virtualFrameIndex)) {
            return;
        }
        m_pipelineStatsQueryPool->reset(virtualFrameIndex);
    }
}

void OceanScene::render(const FrameContext& frameContext) {
    beginPipelineStatsFrame(frameContext.virtualFrameIndex);

    auto* cameraBuffer = m_resourceContext->getRingBuffer("camera");
    cameraBuffer->updateDeviceBuffer(frameContext.commandEncoder);
    m_resourceContext->getRingBuffer(kTonemapBufferId)->updateDeviceBuffer(frameContext.commandEncoder);
    m_resourceContext->getRingBuffer(kAtmosphereBufferId)->updateDeviceBuffer(frameContext.commandEncoder);

    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder);
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
    auto spectrumModel = static_cast<int32_t>(m_oceanParams.spectrumModel);
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
    ImGui::Combo(
        "Tonemap Operator",
        &m_tonemapParams.operatorIndex,
        kTonemapOperatorNames.data(),
        static_cast<int32_t>(kTonemapOperatorNames.size()));
    ImGui::SliderFloat("Exposure", &m_tonemapParams.exposure, 0.05f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::BeginDisabled(m_oceanMeshPipeline == nullptr);
    ImGui::Checkbox("Task + Mesh Shader Path", &m_useMeshShaderPath);
    ImGui::EndDisabled();
    if (ImGui::SliderInt("Clipmap Levels", &m_clipmap.levelCount, 1, kMaxClipmapLevels)) {
        m_resourceContext->getGeometry("ocean").setInstanceCount(
            static_cast<uint32_t>(computeClipmapInstanceCount(m_clipmap)));
    }
    ImGui::Text("Clipmap Radius: %.1f km", computeClipmapRadius(m_clipmap) / kMetersPerKilometer);

    if (ImGui::CollapsingHeader("Pipeline Counters", ImGuiTreeNodeFlags_DefaultOpen)) {
        // What the clipmap describes before anything culls it, so the query results have a
        // denominator that does not itself depend on the view.
        const auto blockQuads = static_cast<uint64_t>(m_clipmap.blockQuads);
        const uint64_t submittedPrimitives =
            static_cast<uint64_t>(computeClipmapInstanceCount(m_clipmap)) * blockQuads * blockQuads * 2;

        const uint64_t emitted = m_pipelineStats.clippingInvocations.value_or(0);
        const uint64_t rasterized = m_pipelineStats.clippingPrimitives.value_or(0);
        const auto percentOf = [](const uint64_t value, const uint64_t total) {
            return total == 0 ? 0.0f : 100.0f * static_cast<float>(value) / static_cast<float>(total);
        };

        ImGui::Text("Submitted prims  (P): %8.3f M", static_cast<double>(submittedPrimitives) / 1.0e6);
        ImGui::Text(
            "Reached clipping (P): %8.3f M  (%.1f%%)",
            static_cast<double>(emitted) / 1.0e6,
            percentOf(emitted, submittedPrimitives));
        ImGui::Text(
            "Rasterized       (P): %8.3f M  (%.1f%%)",
            static_cast<double>(rasterized) / 1.0e6,
            percentOf(rasterized, submittedPrimitives));
        ImGui::Text(
            "VS invocations   (V): %8.3f M",
            static_cast<double>(m_pipelineStats.vertexShaderInvocations.value_or(0)) / 1.0e6);
        ImGui::Text(
            "IA vertices      (V): %8.3f M",
            static_cast<double>(m_pipelineStats.inputAssemblyVertices.value_or(0)) / 1.0e6);

        const auto extent = m_renderer->getSwapChainExtent();
        const auto pixels = static_cast<double>(extent.width) * extent.height;
        const auto fragments = static_cast<double>(m_pipelineStats.fragmentShaderInvocations.value_or(0));
        ImGui::Text(
            "FS invocations   (F): %8.3f M  (%.2fx overdraw)",
            fragments / 1.0e6,
            pixels > 0.0 ? fragments / pixels : 0.0);
        if (m_useMeshShaderPath) {
            ImGui::TextDisabled("Not collected on the mesh path; showing the last vertex-path frame.");
        }
    }
    if (ImGui::Button("Reset View")) {
        resetCamera();
    }
    ImGui::End();

    // The scene builds the LUTs but has no full screen march, so the debug view mode, the sun disk and the
    // fast-path toggles would all be controls over a pass that is not in this graph.
    ImGui::SetNextWindowSize(ImVec2(440.0f, 700.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Atmosphere")) {
        drawAtmosphereGuiContents(
            m_atmosphereSettings,
            m_atmosphereParams,
            {.showDebugViewMode = false, .showSunDisk = false, .showRayMarchingDebug = false});
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
    // The LUTs only; the sky's own full screen march would just be overdrawn by the ocean, and
    // ocean-sky.frag resolves the background from the same sky view LUT the water reflects.
    addAtmosphereLutPasses(*m_renderGraph, *m_renderer, *m_resourceContext, m_atmosphereMaterials);
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
            const auto writeLayer = static_cast<int32_t>(ctx.frameIndex % kFoamLayerCount);

            m_passResources->foam.bind(ctx);
            ctx.commandEncoder.setPushConstants(
                *m_passResources->foam.pipeline->getPipelineLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT,
                FoamPushConstants{
                    .foamGridSize = kFoamGridSize,
                    .foamWindowSize = kFoamWindowSize,
                    .vertexSpacing = kFoamCellSize,
                    .choppiness = m_choppiness,
                    .deltaTime = m_foamDeltaTime,
                    .halfLife = m_foamHalfLife,
                    .injectionThreshold = m_foamInjectionThreshold,
                    .injectionGain = m_foamInjectionGain,
                    .driftVelocity = m_oceanParams.windDirection * m_foamDriftSpeed,
                    .readLayer = 1 - writeLayer,
                    .writeLayer = writeLayer,
                    .anchor = m_foamAnchor,
                    .previousAnchor = m_previousFoamAnchor,
                    .cascadeSizes = cascadeSizes,
                    .cascadeWavelengths = cascadeWavelengths});
            ctx.commandEncoder.dispatchCompute(m_passResources->foam.dispatchSize);
            // Paired with the dispatch rather than with update(), so a dropped frame cannot advance
            // the window without the shader having been told the band it invalidates.
            m_previousFoamAnchor = m_foamAnchor;
        });

    m_renderGraph->addPass(
        kForwardLightingPass,
        PassType::Rasterizer,
        [](rg::RenderGraph::Builder& builder) {
            constexpr auto kOceanMapRead = kVertexSampledRead | kFragmentSampledRead;
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<0>>().image, kOceanMapRead);
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<1>>().image, kOceanMapRead);
            builder.readTexture(builder.getBlackboard().get<FoamPassData>().foam);
            builder.readTexture(builder.getBlackboard().get<TransmittanceLutData>().lut);
            builder.readTexture(builder.getBlackboard().get<SkyViewLutData>().lut);
            builder.readTexture(builder.getBlackboard().get<SkyVolumeLutData>().lut);
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

            ctx.commandEncoder.bindPipeline(*m_skyPipeline);
            ctx.commandEncoder.setViewport(m_renderer->getDefaultViewport());
            ctx.commandEncoder.setScissor(m_renderer->getDefaultScissor());
            ctx.commandEncoder.bindDescriptorSets(m_skyMaterial->getDescriptorSetBinding());
            m_renderer->drawFullScreenQuad(ctx.commandEncoder);

            const bool useMeshPath = m_useMeshShaderPath && m_oceanMeshPipeline != nullptr;
            VulkanPipeline& oceanPipeline = useMeshPath ? *m_oceanMeshPipeline : *m_oceanPipeline;
            Material& oceanMaterial = useMeshPath ? *m_oceanMeshMaterial : *m_oceanMaterial;
            // Has to match the range the layout reflected, or the write lands in no stage at all.
            const VkShaderStageFlags pushConstantStages =
                useMeshPath ? VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT
                            : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            ctx.commandEncoder.bindPipeline(oceanPipeline);
            ctx.commandEncoder.setViewport(m_renderer->getDefaultViewport());
            ctx.commandEncoder.setScissor(m_renderer->getDefaultScissor());
            ctx.commandEncoder.setPushConstants(
                *oceanPipeline.getPipelineLayout(),
                pushConstantStages,
                OceanPushConstants{
                    .clipmapOrigin = m_clipmapOrigin,
                    .clipmapFinestSpacing = m_clipmap.finestSpacing,
                    .clipmapBlockQuads = m_clipmap.blockQuads,
                    .choppiness = m_choppiness,
                    .waterRoughness = m_waterRoughness,
                    .foamThreshold = m_foamThreshold,
                    .foamSoftness = m_foamSoftness,
                    .foamIntensity = m_foamIntensity,
                    .invRmsWaveHeight = 1.0f / std::max(m_rmsWaveHeight, 1e-4f),
                    .slopeVarianceScale = m_slopeVarianceScale,
                    .foamWindowSize = kFoamWindowSize,
                    .foamLayer = static_cast<int32_t>(ctx.frameIndex % kFoamLayerCount),
                    .foamErosion = m_foamErosion,
                    .foamFreshness = m_foamFreshness,
                    // One planet: the horizon the water bends to is the one the sky is built on.
                    .planetRadius = m_atmosphereParams.bottomRadius * kMetersPerKilometer,
                    .cascadeSizes = cascadeSizes,
                    .cascadeWavelengths = cascadeWavelengths,
                    .cascadeSlopeVariances = cascadeSlopeVariances});
            ctx.commandEncoder.bindDescriptorSets(oceanMaterial.getDescriptorSetBinding());
            if (useMeshPath) {
                ctx.commandEncoder.drawMeshTasks(
                    static_cast<uint32_t>(computeClipmapInstanceCount(m_clipmap)) * kOceanTaskGroupsPerBlock);
            } else {
                const bool recordStats =
                    m_pipelineStatsQueryPool != nullptr && !m_pipelineStatsQueryPool->isPending(ctx.virtualFrameIndex);
                if (recordStats) {
                    ctx.commandEncoder.beginQuery(*m_pipelineStatsQueryPool, ctx.virtualFrameIndex);
                }
                m_resourceContext->getGeometry("ocean").bindAndDraw(ctx.commandEncoder);
                if (recordStats) {
                    ctx.commandEncoder.endQuery(*m_pipelineStatsQueryPool, ctx.virtualFrameIndex);
                    m_pipelineStatsQueryPool->setPending(ctx.virtualFrameIndex);
                }
            }
        });

    // addTonemapComputePass is the same curve as a compute dispatch; it measured ~0.07 ms faster and
    // pixel-identical, but stays opt-in.
    addTonemapPass(
        *m_renderGraph,
        *m_renderer,
        *m_resourceContext,
        m_renderGraph->getBlackboard().get<OceanOutputData>().hdrImage,
        // The render test copies the tonemapped image straight out; see Scenes/Test.
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&TonemapPassData::image>());

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
        "ocean", "Ocean.json", {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});
    m_oceanMaterial = m_resourceContext->createMaterial("ocean", m_oceanPipeline);

    if (m_renderer->getDevice().getEnabledFeatures().meshShading) {
        m_oceanMeshPipeline = m_resourceContext->createPipeline(
            "oceanMesh",
            "OceanMesh.json",
            {
                .rasterizationPassDescriptor = m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass),
                .specializationConstants =
                    {
                        {kTilesPerTaskGroupConstantId, kOceanTilesPerTaskGroup},
                        {kTaskGroupsPerBlockConstantId, kOceanTaskGroupsPerBlock},
                        {kTileHeightSigmasConstantId, kOceanTileHeightSigmas},
                    },
            });
        m_oceanMeshMaterial = m_resourceContext->createMaterial("oceanMesh", m_oceanMeshPipeline);
    }

    m_skyPipeline = m_resourceContext->createPipeline(
        "oceanSky", "OceanSky.json", {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});
    m_skyMaterial = m_resourceContext->createMaterial("oceanSky", m_skyPipeline);

    for (Material* material : {m_oceanMaterial, m_oceanMeshMaterial}) {
        if (material == nullptr) {
            continue;
        }
        material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        material->writeDescriptor(1, 0, *m_resourceContext->getRingBuffer("camera"));
        material->writeDescriptor(
            1,
            1,
            m_resourceContext->imageCache.getImage("brdfLut").getView(),
            m_resourceContext->imageCache.getSampler("oceanLinearClamp"));
    }

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

    for (Material* material : {m_oceanMaterial, m_oceanMeshMaterial}) {
        if (material == nullptr) {
            continue;
        }
        material->writeDescriptor(0, 1, finalFftView.operator()<0>(), linearRepeat);
        material->writeDescriptor(0, 2, finalFftView.operator()<1>(), linearRepeat);
        material->writeDescriptor(0, 3, foamView, linearRepeat);
        material->writeDescriptor(0, 4, m_resourceContext->imageCache.getImageView("foamNoiseView"), linearRepeat);
    }

    const auto& blackboard = m_renderGraph->getBlackboard();
    const auto& transmittanceLut = m_renderGraph->getResourceImageView(blackboard.get<TransmittanceLutData>().lut);
    const auto& skyViewLut = m_renderGraph->getResourceImageView(blackboard.get<SkyViewLutData>().lut);
    const auto& skyVolumeLut = m_renderGraph->getResourceImageView(blackboard.get<SkyVolumeLutData>().lut);
    auto& linearClamp = m_resourceContext->imageCache.getSampler("linearClamp");

    for (Material* material : {m_oceanMaterial, m_oceanMeshMaterial}) {
        if (material == nullptr) {
            continue;
        }
        material->writeDescriptor(2, 0, *m_resourceContext->getRingBuffer(kAtmosphereBufferId));
        material->writeDescriptor(2, 1, transmittanceLut, linearClamp);
        material->writeDescriptor(2, 2, skyViewLut, linearClamp);
        material->writeDescriptor(2, 3, skyVolumeLut, linearClamp);
    }

    m_skyMaterial->writeDescriptor(0, 0, *m_resourceContext->getRingBuffer("camera"));
    m_skyMaterial->writeDescriptor(1, 0, *m_resourceContext->getRingBuffer(kAtmosphereBufferId));
    m_skyMaterial->writeDescriptor(1, 1, skyViewLut, linearClamp);

    createAtmosphereLutMaterials(m_atmosphereMaterials, *m_renderGraph, *m_renderer, *m_resourceContext);

    m_renderer->getDevice().flushDescriptorUpdates();
}

} // namespace crisp
