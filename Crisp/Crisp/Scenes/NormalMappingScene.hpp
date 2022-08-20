#pragma once

#include "Scene.hpp"
#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Common/RobinHood.hpp>
#include <Crisp/Math/Headers.hpp>


namespace crisp
{
class Application;

class FreeCameraController;
class TransformBuffer;
class LightSystem;

class ResourceContext;
class RenderGraph;
struct RenderNode;

class NormalMappingScene : public AbstractScene
{
public:
    NormalMappingScene(Renderer* renderer, Application* app);
    ~NormalMappingScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;

private:
    struct PbrUnifMaterialParams
    {
        glm::vec4 albedo;
        float metallic;
        float roughness;
    };

    RenderNode* createRenderNode(std::string id, int transformIndex);

    void createPlane();

    void setupInput();
    void createGui();

    Renderer* m_renderer;
    Application* m_app;

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<RenderGraph> m_renderGraph;
    std::unique_ptr<ResourceContext> m_resourceContext;
    robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    std::unordered_map<std::string, std::size_t> m_renderNodeMap;
    std::vector<std::unique_ptr<RenderNode>> m_renderNodeList;
};
} // namespace crisp
