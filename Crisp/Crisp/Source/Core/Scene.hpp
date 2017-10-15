#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"

#include "Renderer/Transforms.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

#include "vulkan/VulkanSampler.hpp"
#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    class CascadedShadowMapper;
    class Application;
    class UniformColorPipeline;
    class LiquidPipeline;
    class PointSphereSpritePipeline;
    class FullScreenQuadPipeline;
    class BlinnPhongPipeline;
    class ShadowMapPipeline;
    class OutlinePipeline;
    class NormalPipeline;

    class CameraController;
    class InputDispatcher;

    class IndexBuffer;
    class UniformBuffer;
    class TextureView;

    class Skybox;
    class MeshGeometry;

    class SceneRenderPass;
    class ShadowPass;
    class LiquidRenderPass;

    class BoxVisualizer;
    class FluidSimulation;

    namespace gui
    {
        class Label;
    }

    class Scene
    {
    public:
        static constexpr uint32_t SceneRenderPassId = 32;

        Scene(VulkanRenderer* renderer, InputDispatcher* inputDispatcher, Application* app);
        ~Scene();

        void resize(int width, int height);

        void update(float dt);
        void render();

    private:
        void initRenderTargetResources();

        VulkanRenderer* m_renderer;
        VulkanDevice*   m_device;
        Application*    m_app;
        std::unique_ptr<CameraController> m_cameraController;

        uint32_t m_renderMode;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<VulkanSampler> m_nearestNeighborSampler;

        std::unique_ptr<Skybox> m_skybox;

        std::unique_ptr<MeshGeometry> m_sphereGeometry;
        std::unique_ptr<MeshGeometry> m_planeGeometry;
        std::unique_ptr<MeshGeometry> m_bunnyGeometry;

        std::unique_ptr<IndexBuffer> m_indexBuffer;
        uint32_t m_numIndices;

        std::unique_ptr<BoxVisualizer> m_boxVisualizer;
        std::unique_ptr<FluidSimulation> m_fluidSimulation;
        
        std::unique_ptr<VertexBuffer> m_cubeVertexBuffer;
        VertexBufferBindingGroup m_cubeVertexBindingGroup;

        std::unique_ptr<CascadedShadowMapper> m_shadowMapper;

        std::vector<Transforms> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;
        std::unique_ptr<UniformBuffer> m_cameraBuffer;

        std::unique_ptr<SceneRenderPass> m_scenePass;
        std::unique_ptr<BlinnPhongPipeline> m_blinnPhongPipeline;
        std::array<DescriptorSetGroup, VulkanRenderer::NumVirtualFrames> m_blinnPhongDescGroups;

        DescriptorSetGroup m_normalDesc;

        std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;
        DescriptorSetGroup m_sceneDescSetGroup;
        std::unique_ptr<TextureView> m_sceneImageView;

        std::unique_ptr<LiquidRenderPass> m_liquidPass;

        std::unique_ptr<NormalPipeline> m_liquidNormalPipeline;
        std::unique_ptr<LiquidPipeline> m_liquidPipeline;

        std::array<DescriptorSetGroup, VulkanRenderer::NumVirtualFrames> m_liquidDesc;
    };
}
