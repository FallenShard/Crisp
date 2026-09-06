#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/PathTracer.hpp>
#include <Crisp/Scenes/RayTracingSceneData.hpp>
#include <Crisp/Scenes/RayTracingSceneParser.hpp>
#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Vulkan/RayTracingPipelineBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>
#include <Crisp/Vulkan/VulkanDescriptorHeap.hpp>
#include <Crisp/Vulkan/VulkanStagingBelt.hpp>

namespace crisp {
class VulkanRayTracingScene : public Scene {
public:
    VulkanRayTracingScene(
        Renderer* renderer, Window* window, std::filesystem::path outputDir, const nlohmann::json& args);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    void buildRenderGraph();
    void updateDescriptorHeap();
    void traceRays(const FrameContext& frameContext);

    void setupInput();

    std::filesystem::path m_outputDir;

    std::unique_ptr<FreeCameraController> m_cameraController;

    std::unique_ptr<PathTracer> m_pathTracer;

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    AsyncReadback m_screenshot;
    bool m_screenshotRequested{false};
    bool m_closeAfterScreenshot{false};
    int32_t m_captureAfterSamples{0};
    int32_t m_samplesPerFrame{1};
    std::filesystem::path m_screenshotFilename{"screenshot.exr"};
    glm::ivec2 m_renderResolution{1920, 1080};

    std::unique_ptr<VulkanImage> m_environmentImage;
    std::vector<std::unique_ptr<VulkanImage>> m_materialImages;

    struct IntegratorParameters {
        int32_t maxBounces{32};
        int32_t sampleCount{1};
        int32_t frameIdx{0};
        int32_t sampleOffset{0};
        uint32_t seed{0};
        int32_t reconstructionFilter{static_cast<int32_t>(ReconstructionFilterType::Box)};
        int32_t lightCount{0};
        int32_t shapeCount{0};
        int32_t samplingMode{0};
        int32_t environmentEnabled{0};
        int32_t environmentWidth{0};
        int32_t environmentHeight{0};
        float environmentScale{1.0f};
    };

    static_assert(sizeof(IntegratorParameters) == 52);

    SceneDescription m_sceneDesc;
    RayTracingSceneAddresses m_sceneAddresses;

    IntegratorParameters m_integratorParams;

    VulkanBuffer* m_brdfParamsBuffer{};
    VulkanBuffer* m_lightParamsBuffer{};
    VulkanBuffer* m_instancePropsBuffer{};
    VulkanBuffer* m_environmentCdfBuffer{};
};
} // namespace crisp
