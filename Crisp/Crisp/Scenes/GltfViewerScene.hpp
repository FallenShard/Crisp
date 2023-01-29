#pragma once

#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Scenes/Scene.hpp>

#include <Crisp/Common/HashMap.hpp>
#include <Crisp/Core/ConnectionHandler.hpp>
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

class GltfViewerScene : public AbstractScene
{
public:
    GltfViewerScene(Renderer* renderer, Application* app);
    ~GltfViewerScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;

private:
    RenderNode* createRenderNode(std::string nodeId, bool hasTransform);

    void createCommonTextures();
    void setEnvironmentMap(const std::string& envMapName);

    void loadGltf();

    void setupInput();

    std::vector<ConnectionHandler> m_connectionHandlers;

    std::unique_ptr<TargetCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    PbrParams m_uniformMaterialParams;
    std::unique_ptr<Skybox> m_skybox;
};
} // namespace crisp
