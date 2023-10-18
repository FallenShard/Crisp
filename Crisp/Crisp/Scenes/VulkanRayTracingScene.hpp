#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>

#include <Crisp/Renderer/RayTracingPipelineBuilder.hpp>
#include <Crisp/Vulkan/VulkanAccelerationStructure.hpp>

namespace crisp {
class VulkanRayTracingScene : public Scene {
public:
    VulkanRayTracingScene(Renderer* renderer, Window* window);

    void resize(int width, int height) override;
    void update(float dt) override;
    void render() override;

private:
    void renderGui() override;

    struct PbrUnifMaterialParams {
        glm::vec4 albedo;
        float metallic;
        float roughness;
    };

    std::unique_ptr<VulkanPipeline> createPipeline();
    void updateDescriptorSets();

    void setupInput();

    void updateGeometryBufferDescriptors(const Geometry& geometry, uint32_t idx);

    std::unique_ptr<FreeCameraController> m_cameraController;

    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_bottomLevelAccelStructures;
    std::unique_ptr<VulkanAccelerationStructure> m_topLevelAccelStructure;

    std::unique_ptr<VulkanImage> m_rtImage;
    std::vector<std::unique_ptr<VulkanImageView>> m_rtImageViews;

    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<Material> m_material;

    ShaderBindingTable m_shaderBindingTable;

    struct IntegratorParameters {
        int maxBounces{32};
        int sampleCount{10};
        int frameIdx{0};
    };

    IntegratorParameters m_integratorParams;
};
} // namespace crisp
