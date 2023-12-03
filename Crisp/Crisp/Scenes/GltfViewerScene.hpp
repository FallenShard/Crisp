#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Core/ConnectionHandler.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Mesh/SkinningData.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {
class GltfViewerScene : public Scene {
public:
    GltfViewerScene(Renderer* renderer, Window* window);

    void resize(int width, int height) override;
    void update(float dt) override;
    void render() override;
    void renderGui() override;

private:
    RenderNode* createRenderNode(std::string nodeId, bool hasTransform);

    void createCommonTextures();

    void loadGltf(const std::string& gltfAsset);

    void setupInput();

    std::unique_ptr<rg::RenderGraph> m_rg;
    std::vector<std::unique_ptr<VulkanImageView>> m_sceneImageViews;

    std::vector<ConnectionHandler> m_connectionHandlers;

    std::unique_ptr<TargetCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    PbrParams m_uniformMaterialParams;
    std::unique_ptr<Skybox> m_skybox;

    SkinningData m_skinningData;
    AnimationData m_animation;
};
} // namespace crisp
