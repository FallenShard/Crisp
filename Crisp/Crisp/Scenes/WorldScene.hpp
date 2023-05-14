#pragma once

#include "Renderer/Renderer.hpp"
#include "Scene.hpp"

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/ConnectionHandler.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Math/Headers.hpp>

namespace crisp
{
namespace gui
{
class Form;
}

class Application;
class CameraController;

class TransformBuffer;
class LightSystem;

class ResourceContext;
class RenderGraph;
struct RenderNode;

class VulkanDevice;

class VulkanSampler;
class BoxVisualizer;
class Skybox;

class WorldScene : public AbstractScene
{
public:
    WorldScene(Renderer* renderer, Application* app);
    ~WorldScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;

    void setRedAlbedo(double red);
    void setGreenAlbedo(double green);
    void setBlueAlbedo(double blue);
    void setMetallic(double metallic);
    void setRoughness(double roughness);
    void setUScale(double uScale);
    void setVScale(double vScale);
    void onMaterialSelected(const std::string& material);

private:
    RenderNode* createRenderNode(std::string id, int transformIndex);

    void createCommonTextures();

    Material* createPbrTexMaterial(const std::string& type, const std::string& tag);
    void createShaderballs();
    void createPlane();

    void setupInput();
    void createGui(gui::Form* form);

    std::vector<ConnectionHandler> m_connectionHandlers;

    std::unique_ptr<CameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    PbrUnifMaterialParams m_uniformMaterialParams;
    std::unique_ptr<Skybox> m_skybox;

    glm::vec2 m_shaderBallUVScale;
};
} // namespace crisp
