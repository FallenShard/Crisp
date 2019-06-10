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

    class TerrainScene : public AbstractScene
    {
    public:
        TerrainScene(Renderer* renderer, Application* app);
        ~TerrainScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

    private:
        Renderer*    m_renderer;
        Application* m_app;

        std::unique_ptr<CameraController> m_cameraController;
        std::unique_ptr<UniformBuffer>    m_cameraBuffer;

        std::vector<std::unique_ptr<Material>> m_materials;
        std::vector<std::unique_ptr<Geometry>> m_geometries;
        std::vector<std::unique_ptr<VulkanPipeline>> m_pipelines;

        std::vector<std::unique_ptr<VulkanImage>>     m_images;
        std::vector<std::unique_ptr<VulkanImageView>> m_imageViews;

        TransformPack m_transforms;
        std::unique_ptr<UniformBuffer> m_transformBuffer;

        std::unique_ptr<RenderGraph> m_renderGraph;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;

        int m_numTiles;
    };
}