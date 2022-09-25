#pragma once

#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>

#include <Crisp/Common/HashMap.hpp>
#include <Crisp/ConnectionHandler.hpp>
#include <Crisp/Math/Headers.hpp>

namespace crisp
{
namespace gui
{
class Form;
}

class Application;
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

    void setRedAlbedo(double red);
    void setGreenAlbedo(double green);
    void setBlueAlbedo(double blue);
    void setMetallic(double metallic);
    void setRoughness(double roughness);
    void setUScale(double uScale);
    void setVScale(double vScale);
    void onMaterialSelected(const std::string& material);

private:
    RenderNode* createRenderNode(std::string id, std::optional<int> transformIndex);

    void createCommonTextures();

    void createShaderballs();
    void createPlane();

    void setupInput();
    void createGui(gui::Form* form);

    Renderer* m_renderer;
    Application* m_app;

    std::vector<ConnectionHandler> m_connectionHandlers;

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<RenderGraph> m_renderGraph;
    std::unique_ptr<ResourceContext> m_resourceContext;
    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    std::unique_ptr<VulkanImage> m_multiScatTex;
    std::vector<std::unique_ptr<VulkanImageView>> m_multiScatTexViews;

    PbrParams m_uniformMaterialParams;
    std::unique_ptr<Skybox> m_skybox;
};
} // namespace crisp
