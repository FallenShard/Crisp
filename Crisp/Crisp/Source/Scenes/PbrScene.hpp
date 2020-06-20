#pragma once

#include "Scene.hpp"
#include "Renderer/Renderer.hpp"

#include <CrispCore/Math/Headers.hpp>

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

    class PbrScene : public AbstractScene
    {
    public:
        PbrScene(Renderer* renderer, Application* app);
        ~PbrScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
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
        struct PbrUnifMaterialParams
        {
            glm::vec4 albedo;
            float metallic;
            float roughness;
        };

        RenderNode* createRenderNode(std::string id, int transformIndex);

        void createCommonTextures();

        Material* createPbrTexMaterial(const std::string& type, const std::string& tag);
        void createSkyboxNode();
        void createShaderball();
        void createPlane();

        void setupInput();
        void createGui(gui::Form* form);

        Renderer*    m_renderer;
        Application* m_app;

        std::unique_ptr<CameraController> m_cameraController;
        std::unique_ptr<LightSystem>      m_lightSystem;
        std::unique_ptr<TransformBuffer>  m_transformBuffer;

        std::unique_ptr<RenderGraph> m_renderGraph;
        std::unique_ptr<ResourceContext> m_resourceContext;
        std::unordered_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

        PbrUnifMaterialParams m_uniformMaterialParams;
        std::unique_ptr<Skybox> m_skybox;

        glm::vec2 m_shaderBallUVScale;
    };
}
