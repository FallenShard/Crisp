#pragma once

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/DescriptorSetGroup.hpp"
#include "Geometry/TransformPack.hpp"
#include "Scene.hpp"

namespace crisp
{
    class Application;

    class CameraController;

    class VulkanRenderer;
    class VulkanDevice;
    class VulkanSampler;
    class VulkanPipeline;
    class VulkanRenderPass;
    class VulkanImageView;

    class UniformBuffer;
    class MeshGeometry;
    class Skybox;

    class SceneRenderPass;

    class PhysicallyBasedMaterialsScene : public Scene
    {
    public:
        PhysicallyBasedMaterialsScene(VulkanRenderer* renderer, Application* app);
        ~PhysicallyBasedMaterialsScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt) override;

        virtual void render() override;

    private:
        VulkanRenderer* m_renderer;
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
        std::array<DescriptorSetGroup, VulkanRenderer::NumVirtualFrames> m_physBasedDesc;

        // Scene to screen compositing
        std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;
        DescriptorSetGroup m_sceneDescSetGroup;
        std::unique_ptr<VulkanImageView> m_sceneImageView;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
    };
}