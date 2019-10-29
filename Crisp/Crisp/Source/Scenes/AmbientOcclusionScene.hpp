#pragma once

#include "Scenes/Scene.hpp"
#include "Geometry/TransformPack.hpp"

#include <vector>
#include <memory>

namespace crisp
{
    class Application;
    class Renderer;
    class CameraController;
    class RenderGraph;

    class UniformBuffer;
    class Material;
    class Geometry;

    class VulkanPipeline;
    class VulkanImage;
    class VulkanImageView;
    class VulkanSampler;

    class Skybox;

    class AmbientOcclusionScene : public AbstractScene
    {
    public:
        AmbientOcclusionScene(Renderer* renderer, Application* app);
        ~AmbientOcclusionScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

        void setNumSamples(int numSamples);
        void setRadius(double radius);

    private:
        struct SsaoParams
        {
            int numSamples;
            float radius;
        };

        Renderer*    m_renderer;
        Application* m_app;

        std::unique_ptr<CameraController> m_cameraController;
        std::unique_ptr<UniformBuffer>    m_cameraBuffer;

        std::vector<std::unique_ptr<Material>> m_materials;
        std::vector<std::unique_ptr<Geometry>> m_geometries;
        std::vector<std::unique_ptr<VulkanPipeline>> m_pipelines;

        std::vector<std::unique_ptr<VulkanImage>>     m_images;
        std::vector<std::unique_ptr<VulkanImageView>> m_imageViews;

        std::vector<TransformPack> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformBuffer;

        std::unique_ptr<RenderGraph> m_renderGraph;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<VulkanSampler> m_nearestSampler;
        std::unique_ptr<VulkanSampler> m_noiseSampler;

        std::unique_ptr<Skybox> m_skybox;

        SsaoParams m_ssaoParams;
        std::unique_ptr<UniformBuffer> m_sampleBuffer;
    };
}