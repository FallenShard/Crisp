#pragma once

#include "Scene.hpp"
#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Math/Headers.hpp>

namespace crisp
{
namespace gui
{
class Form;
}

class FreeCameraController;

class TransformBuffer;
class LightSystem;

class ResourceContext;
class RenderGraph;
struct RenderNode;

class VulkanDevice;

class VulkanSampler;
class BoxVisualizer;
class Skybox;

class ClusteredLightingScene : public Scene
{
public:
    ClusteredLightingScene(Renderer* renderer, Window* window);
    ~ClusteredLightingScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;

    void setRedAlbedo(double red);
    void setGreenAlbedo(double green);
    void setBlueAlbedo(double blue);
    void setMetallic(double metallic);
    void setRoughness(double roughness);

private:
    struct PbrUnifMaterialParams
    {
        glm::vec4 albedo;
        float metallic;
        float roughness;
    };

    RenderNode* createRenderNode(std::string id, int transformIndex);

    void createCommonTextures();

    void createShaderball();
    void createPlane();

    void setupInput();
    void createGui(gui::Form* form);

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    PbrUnifMaterialParams m_uniformMaterialParams;
};
} // namespace crisp
