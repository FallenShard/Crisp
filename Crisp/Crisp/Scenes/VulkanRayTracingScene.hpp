#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/RayTracingPipelineBuilder.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/RayTracingSceneParser.hpp>
#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>

namespace crisp {
class VulkanRayTracingScene : public Scene {
public:
    VulkanRayTracingScene(Renderer* renderer, Window* window);

    void resize(int width, int height) override;
    void update(float dt) override;
    void render() override;

private:
    void renderGui() override;

    std::unique_ptr<VulkanPipeline> createPipeline();
    void updateDescriptorSets();

    void setupInput();

    std::unique_ptr<FreeCameraController> m_cameraController;

    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_bottomLevelAccelStructures;
    std::unique_ptr<VulkanAccelerationStructure> m_topLevelAccelStructure;

    std::unique_ptr<VulkanImage> m_rtImage;
    std::vector<std::unique_ptr<VulkanImageView>> m_rtImageViews;
    std::shared_ptr<StagingVulkanBuffer> m_screenshotBuffer;

    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<Material> m_material;

    ShaderBindingTable m_shaderBindingTable;

    struct IntegratorParameters {
        int32_t maxBounces{32};
        int32_t sampleCount{10};
        int32_t frameIdx{0};
        int32_t lightCount{0};
        int32_t shapeCount{0};
        int32_t samplingMode{2};
    };

    SceneDescription m_sceneDesc;

    IntegratorParameters m_integratorParams;

    bool m_screenshotRequested = false;
};
} // namespace crisp
