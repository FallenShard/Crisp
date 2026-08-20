#pragma once

#include <array>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Models/Atmosphere.hpp>
#include <Crisp/Models/Ocean.hpp>
#include <Crisp/Models/Tonemap.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipelineStatsQueryPool.hpp>

namespace crisp {
struct OceanPassResources;

class OceanScene : public Scene {
public:
    OceanScene(Renderer* renderer, Window* window);
    ~OceanScene() override;

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    void setupInput();
    void setupResources();
    void buildRenderGraph();
    void resetCamera();
    void writeGraphDependentDescriptors();
    void beginPipelineStatsFrame(uint32_t virtualFrameIndex);

    std::unique_ptr<VulkanImage> createInitialSpectrum();
    std::unique_ptr<VulkanImage> createFoamNoise();

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<OceanPassResources> m_passResources;

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<TransformBuffer> m_transformBuffer;
    TransformHandle m_transformHandle{TransformHandle::createInvalidHandle()};
    VulkanPipeline* m_oceanPipeline{nullptr};
    Material* m_oceanMaterial{nullptr};
    std::unique_ptr<VulkanPipelineStatsQueryPool> m_pipelineStatsQueryPool;
    PipelineStats m_pipelineStats{};
    VulkanPipeline* m_skyPipeline{nullptr};
    Material* m_skyMaterial{nullptr};

    OceanParameters m_oceanParams;
    // Drives the sky, the reflected ray and the sun colour from one set of LUTs; docs/ocean.md 13.
    AtmosphereMaterials m_atmosphereMaterials;
    AtmosphereParameters m_atmosphereParams{};
    std::array<OceanCascade, kOceanCascadeCount> m_cascades;
    std::array<OceanCascadeMoments, kOceanCascadeCount> m_unitAmplitudeMoments{};
    std::array<OceanCascadeMoments, kOceanCascadeCount> m_cascadeMoments{};
    TonemapParameters m_tonemapParams{};
    float m_choppiness;
    float m_waterRoughness{0.08f};
    float m_foamThreshold{0.2f};
    float m_foamSoftness{0.1f};
    float m_foamIntensity{1.0f};

    // Accumulated foam: injected where the surface folds, decayed with a half-life in seconds, and
    // drifted by the wind. See docs/ocean.md item 11.
    float m_foamHalfLife{2.5f};
    float m_foamInjectionThreshold{1.0f};
    float m_foamInjectionGain{1.5f};
    float m_foamDriftSpeed{0.6f};
    float m_foamDeltaTime{0.0f};
    // Erosion tears the foam boundary; freshness makes decayed foam thin into streaks rather than
    // dim uniformly.
    float m_foamErosion{0.35f};
    float m_foamFreshness{1.2f};

    float m_slopeVarianceScale{1.0f};

    // Azimuth 225 puts the sun opposite the default camera, so its glitter path lands in frame; the water is
    // otherwise lit from behind the viewer and reads almost black.
    AtmosphereSettings m_atmosphereSettings{.sunAzimuthDegrees = 225.0f, .sunElevationDegrees = 35.0f};

    float m_rmsWaveHeight{1.0f};
    bool m_spectrumDirty{true};

    // Camera-centred nested rings; see docs/ocean.md item 17.
    OceanClipmap m_clipmap{};
    glm::vec2 m_clipmapOrigin{0.0f};

    glm::vec2 m_foamAnchor{0.0f};
    glm::vec2 m_previousFoamAnchor{1.0e9f};

    bool m_paused{false};
};
} // namespace crisp
