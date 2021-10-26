#pragma once

#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Geometry/TransformPack.hpp>

#include <vector>
#include <memory>
#include <string>

namespace crisp
{
    class Application;
    class Renderer;
    class FreeCameraController;
    class RenderGraph;
    class ResourceContext;

    class TransformBuffer;

    class UniformBuffer;
    class Material;
    class Geometry;

    class VulkanPipeline;
    class VulkanImage;
    class VulkanImageView;
    class VulkanSampler;
    class RenderNode;

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
        std::unique_ptr<RenderNode> createPostProcessingEffect(std::string renderPassName, std::string pipelineLuaFile);
        void createGui();

        struct SsaoParams
        {
            int numSamples;
            float radius;
        };

        Application* m_app;

        Renderer*    m_renderer;
        std::unique_ptr<ResourceContext> m_resourceContext;


        std::unique_ptr<FreeCameraController> m_cameraController;
        std::unique_ptr<UniformBuffer>    m_cameraBuffer;

        std::vector<std::unique_ptr<Geometry>> m_geometries;

        std::vector<std::unique_ptr<VulkanImage>>     m_images;
        std::vector<std::unique_ptr<VulkanImageView>> m_imageViews;

        std::unique_ptr<TransformBuffer> m_transformBuffer;

        std::unique_ptr<RenderGraph> m_renderGraph;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<VulkanSampler> m_nearestSampler;
        std::unique_ptr<VulkanSampler> m_noiseSampler;

        std::unique_ptr<Skybox> m_skybox;

        SsaoParams m_ssaoParams;
        std::unique_ptr<UniformBuffer> m_sampleBuffer;

        std::unique_ptr<RenderNode> m_floorNode;
        std::unique_ptr<RenderNode> m_sponzaNode;
        std::unique_ptr<RenderNode> m_ssaoNode;
        std::unique_ptr<RenderNode> m_blurHNode;
        std::unique_ptr<RenderNode> m_blurVNode;
    };
}