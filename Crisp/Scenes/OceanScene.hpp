#pragma once

#include "Scene.hpp"
#include "Renderer/Renderer.hpp"

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    class Application;
    class CameraController;

    class TransformBuffer;

    class ResourceContext;
    class RenderGraph;
    struct RenderNode;

    class VulkanImage;

    class OceanScene : public AbstractScene
    {
    public:
        OceanScene(Renderer* renderer, Application* app);
        ~OceanScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

    private:
        std::unique_ptr<VulkanImage> createInitialSpectrum();
        int applyFFT(std::string image);

        Application* m_app;

        Renderer* m_renderer;
        std::unique_ptr<ResourceContext> m_resourceContext;
        std::unique_ptr<RenderGraph>     m_renderGraph;

        std::unique_ptr<CameraController> m_cameraController;
        std::unique_ptr<TransformBuffer>  m_transformBuffer;

        std::vector<std::unique_ptr<RenderNode>> m_renderNodes;

        float m_time = 0.0;
    };
}
