#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Core/ConnectionHandler.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraphExperimental.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{
class PbrScene : public Scene
{
public:
    PbrScene(Renderer* renderer, Window* window);

    void resize(int width, int height) override;
    void update(float dt) override;
    void render() override;
    void renderGui() override;

    void onMaterialSelected(const std::string& material);

private:
    RenderNode* createRenderNode(std::string nodeId, bool hasTransform);

    void createCommonTextures();
    void setEnvironmentMap(const std::string& envMapName);

    void createSceneObject();
    void createPlane();

    void setupInput();

    void updateMaterialsWithRenderGraphResources();

    std::unique_ptr<rg::RenderGraph> m_rg;
    std::vector<std::unique_ptr<VulkanImageView>> m_sceneImageViews;

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;

    std::unique_ptr<TransformBuffer> m_transformBuffer;

    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    PbrParams m_uniformMaterialParams;
    std::unique_ptr<Skybox> m_skybox;

    std::string m_shaderBallPbrMaterialKey;

    std::vector<std::string> m_environmentMapNames;

    bool m_showFloor{true};
};
} // namespace crisp
