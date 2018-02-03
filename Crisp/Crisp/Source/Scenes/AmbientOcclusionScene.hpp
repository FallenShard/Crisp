#pragma once

#include <memory>
#include <array>

#include "Scene.hpp"

#include "Renderer/Transforms.hpp"
#include "Renderer/DescriptorSetGroup.hpp"
#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    class Application;
    class VulkanRenderer;
    class VulkanDevice;

    class CameraController;
    class SceneRenderPass;
    class AmbientOcclusionPass;
    class UniformBuffer;
    class VulkanSampler;
    class TextureView;
    class Texture;
    
    class VulkanPipeline;
    class MeshGeometry;

    class Skybox;

    class AmbientOcclusionScene : public Scene
    {
    public:
        AmbientOcclusionScene(VulkanRenderer* renderer, Application* app);
        ~AmbientOcclusionScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

        void setNumSamples(int value);
        void setRadius(double radius);

    private:
        VulkanRenderer*  m_renderer;
        VulkanDevice*    m_device;
        Application*     m_app;

        std::unique_ptr<CameraController> m_cameraController;

        std::unique_ptr<AmbientOcclusionPass> m_aoPass;
        std::unique_ptr<VulkanPipeline> m_ssaoPipeline;
        std::array<DescriptorSetGroup, VulkanRenderer::NumVirtualFrames> m_ssaoSetGroups;

        std::unique_ptr<SceneRenderPass> m_scenePass;
        std::unique_ptr<UniformBuffer> m_cameraBuffer;
        std::unique_ptr<UniformBuffer> m_samplesBuffer;

        std::unique_ptr<VulkanSampler> m_nearestSampler;

        std::vector<Transforms> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        std::unique_ptr<VulkanPipeline> m_uniformColorPipeline;
        DescriptorSetGroup m_unifColorSetGroup;

        std::unique_ptr<VulkanPipeline> m_normalPipeline;
        DescriptorSetGroup m_normalSetGroup;

        std::unique_ptr<VulkanPipeline> m_fsQuadPipeline;
        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<TextureView> m_sceneImageView;
        DescriptorSetGroup m_sceneDescSetGroup;

        std::unique_ptr<MeshGeometry> m_planeGeometry;
        std::unique_ptr<MeshGeometry> m_buddhaGeometry;

        std::unique_ptr<Skybox> m_skybox;

        std::unique_ptr<Texture>       m_noiseTex;
        std::unique_ptr<TextureView>   m_noiseMapView;
        std::unique_ptr<VulkanSampler> m_noiseSampler;

        int m_numSamples;
        float m_radius;
    };
}