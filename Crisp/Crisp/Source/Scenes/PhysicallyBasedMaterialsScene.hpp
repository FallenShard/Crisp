#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/DescriptorSetGroup.hpp"
#include "Geometry/TransformPack.hpp"
#include "Scene.hpp"

#include "Renderer/Material.hpp"

namespace crisp
{
    class Application;

    class CameraController;

    class Renderer;
    class RenderGraph;
    class VulkanDevice;
    class VulkanSampler;
    class VulkanPipeline;
    class VulkanRenderPass;
    class VulkanImageView;

    class UniformBuffer;
    class MeshGeometry;
    class Skybox;

    class SceneRenderPass;

    class PhysicallyBasedMaterialsScene : public AbstractScene
    {
    public:
        PhysicallyBasedMaterialsScene(Renderer* renderer, Application* app);
        ~PhysicallyBasedMaterialsScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt) override;

        virtual void render() override;

    private:
        std::unique_ptr<Material> createPbrMaterial(const std::string& type);

        Renderer*    m_renderer;
        Application* m_app;

        std::unique_ptr<CameraController> m_cameraController;
        std::unique_ptr<UniformBuffer>    m_cameraBuffer;

        std::vector<TransformPack>     m_transforms;
        std::unique_ptr<UniformBuffer> m_transformBuffer;

        std::unique_ptr<RenderGraph> m_renderGraph;

        std::vector<std::unique_ptr<VulkanImage>>     m_images;
        std::vector<std::unique_ptr<VulkanImageView>> m_imageViews;

        std::vector<std::unique_ptr<Material>> m_materials;
        std::vector<std::unique_ptr<Geometry>> m_geometries;

        std::unique_ptr<VulkanPipeline> m_physBasedPipeline;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
    };
}