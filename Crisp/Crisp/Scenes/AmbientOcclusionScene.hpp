#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {

class AmbientOcclusionScene : public Scene {
public:
    AmbientOcclusionScene(Renderer* renderer, Window* window);

    void resize(int width, int height) override;
    void update(float dt) override;
    void render() override;
    void renderGui() override;

private:
    struct SsaoParams {
        int sampleCount;
        float radius;
    };

    std::unique_ptr<rg::RenderGraph> m_rg;
    std::vector<std::unique_ptr<VulkanImageView>> m_sceneImageViews;

    std::unique_ptr<FreeCameraController> m_cameraController;

    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<Skybox> m_skybox;

    SsaoParams m_ssaoParams;

    // RenderNode m_ambientOcclusionNode;
    // RenderNode m_horizontalBlurNode;
    // RenderNode m_verticalBlurNode;

    std::unique_ptr<RenderNode> m_floorNode;
    std::unique_ptr<RenderNode> m_sponzaNode;
};
} // namespace crisp