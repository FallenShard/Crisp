#pragma once

#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Scenes/Scene.hpp>

#include <Crisp/Core/ConnectionHandler.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Math/Headers.hpp>

namespace crisp
{
namespace gui
{
class Form;
}

class FreeCameraController;
class TargetCameraController;

class TransformBuffer;
class LightSystem;

class ResourceContext;
class RenderGraph;
struct RenderNode;

class VulkanDevice;

class VulkanImage;
class VulkanImageView;
class VulkanSampler;
class BoxVisualizer;
class Skybox;

class PbrScene : public AbstractScene
{
public:
    PbrScene(Renderer* renderer, Application* app);
    ~PbrScene();

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

    void createShaderball();
    void createPlane();

    void setupInput();
    void createGui(gui::Form* form);

    std::vector<ConnectionHandler> m_connectionHandlers;

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    PbrParams m_uniformMaterialParams;
    std::unique_ptr<Skybox> m_skybox;

    std::string m_shaderBallPbrMaterialKey;
};
} // namespace crisp
