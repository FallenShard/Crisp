#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/DescriptorSetGroup.hpp"
#include "Geometry/TransformPack.hpp"
#include "Scene.hpp"

namespace crisp
{
    class Application;

    class CameraController;

    class Renderer;
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
        Renderer* m_renderer;
        VulkanDevice*   m_device;
        Application*    m_app;

        std::unique_ptr<CameraController> m_cameraController;
        std::unique_ptr<UniformBuffer>    m_cameraTransformBuffer;

        std::vector<TransformPack>     m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        std::unique_ptr<Texture>     m_materialTex;
        std::unique_ptr<VulkanImageView> m_materialTexView;

        std::unique_ptr<Texture>     m_roughnessTex;
        std::unique_ptr<VulkanImageView> m_roughnessTexView;

        std::unique_ptr<Texture>     m_metallicTex;
        std::unique_ptr<VulkanImageView> m_metallicTexView;

        std::unique_ptr<MeshGeometry> m_sphereMesh;

        std::unique_ptr<SceneRenderPass> m_mainPass;
        std::unique_ptr<VulkanPipeline> m_physBasedPipeline;
        std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_physBasedDesc;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
    };
}