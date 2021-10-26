#pragma once

#include "Scene.hpp"
#include <Crisp/Renderer/Renderer.hpp>

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    class Application;

    class FreeCameraController;
    class TransformBuffer;
    class LightSystem;

    class ResourceContext;
    class RenderGraph;
    struct RenderNode;

    class VulkanRayTracer;
    class RayTracingMaterial;

    class VulkanRayTracingScene : public AbstractScene
    {
    public:
        VulkanRayTracingScene(Renderer* renderer, Application* app);
        ~VulkanRayTracingScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
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
        std::unique_ptr<LightSystem>      m_lightSystem;
        std::unique_ptr<TransformBuffer>  m_transformBuffer;

        std::unique_ptr<RenderGraph>     m_renderGraph;
        std::unique_ptr<ResourceContext> m_resourceContext;
        std::unordered_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

        std::unique_ptr<VulkanRayTracer> m_rayTracer;
        std::unique_ptr<RayTracingMaterial> m_rayTracingMaterial;
    };
}
