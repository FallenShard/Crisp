#pragma once

#include <vector>
#include <memory>
#include <array>

#include "Scenes/Scene.hpp"
#include "Geometry/TransformPack.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class Application;
    class CameraController;

    class FluidSimulation;

    class SceneRenderPass;
    class VulkanPipeline;
    class VulkanImageView;
    class UniformBuffer;
    class VertexBuffer;
    class IndexBuffer;
    class Texture;
    class VulkanDevice;
    class Renderer;
    class VulkanSampler;

    class TerrainScene : public Scene
    {
    public:
        TerrainScene(Renderer* renderer, Application* app);
        ~TerrainScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

    private:
        Renderer*     m_renderer;
        VulkanDevice* m_device;
        Application*  m_app;

        std::unique_ptr<CameraController> m_cameraController;

        std::unique_ptr<VertexBuffer> m_vertexBuffer;
        std::unique_ptr<IndexBuffer> m_indexBuffer;

        std::unique_ptr<VulkanPipeline> m_terrainPipeline;
        DescriptorSetGroup m_terrainDescGroup;

        TransformPack m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        std::unique_ptr<Texture> m_heightMap;
        std::unique_ptr<VulkanImageView> m_heightMapView;

        std::unique_ptr<SceneRenderPass> m_scenePass;
        std::unique_ptr<UniformBuffer> m_cameraBuffer;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        int m_numTiles;
    };
}