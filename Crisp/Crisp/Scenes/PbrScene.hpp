#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Core/ConnectionHandler.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{
class PbrScene : public AbstractScene
{
public:
    PbrScene(Renderer* renderer, Window* window);

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;
    virtual void renderGui() override;

    void setRedAlbedo(double red);
    void setGreenAlbedo(double green);
    void setBlueAlbedo(double blue);
    void setMetallic(double metallic);
    void setRoughness(double roughness);
    void setUScale(double uScale);
    void setVScale(double vScale);
    void onMaterialSelected(const std::string& material);

private:
    RenderNode* createRenderNode(std::string nodeId, bool hasTransform);

    void createCommonTextures();
    void setEnvironmentMap(const std::string& envMapName);

    void createSceneObject();
    void createPlane();

    void setupInput();

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
