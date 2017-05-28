#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"

#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class CascadedShadowMapper;
    class Application;
    class VulkanRenderer;
    class UniformColorPipeline;
    class LiquidPipeline;
    class PointSphereSpritePipeline;
    class FullScreenQuadPipeline;
    class BlinnPhongPipeline;
    class ShadowMapPipeline;

    class CameraController;
    class InputDispatcher;

    class IndexBuffer;
    class UniformBuffer;
    class TextureView;

    class Skybox;
    class MeshGeometry;

    class SceneRenderPass;
    class ShadowPass;

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

        
        //void calculateFrustumOBB(ShadowParameters& shadowParams, float zNear, float zFar);

        VulkanRenderer* m_renderer;
        VulkanDevice* m_device;
        Application* m_app;
        std::unique_ptr<CameraController> m_cameraController;

        VkSampler m_linearClampSampler;

        std::unique_ptr<Skybox> m_skybox;

        std::unique_ptr<MeshGeometry> m_sphereGeometry;
        std::unique_ptr<MeshGeometry> m_planeGeometry;
        std::unique_ptr<MeshGeometry> m_bunnyGeometry;

        struct Transforms
        {
            glm::mat4 MVP;
            glm::mat4 MV;
            glm::mat4 M;
            glm::mat4 N;
        };

        std::unique_ptr<CascadedShadowMapper> m_shadowMapper;

        //ShadowParameters m_shadowParameters[3];

        std::vector<Transforms> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;
        std::unique_ptr<UniformBuffer> m_cameraBuffer;
        //std::unique_ptr<UniformBuffer> m_shadowTransformsBuffer;
        //std::vector<std::shared_ptr<TextureView>> m_shadowMapViews;

        std::unique_ptr<SceneRenderPass> m_scenePass;

        std::unique_ptr<BlinnPhongPipeline> m_blinnPhongPipeline;
        DescriptorSetGroup m_blinnPhongDescGroups[3];

        //std::unique_ptr<PointSphereSpritePipeline> m_psPipeline;
        //std::vector<DescriptorSetGroup> m_descriptorSetGroups;
        //
        //std::unique_ptr<LiquidPipeline> m_dielectricPipeline;
        //std::unique_ptr<ShadowPass> m_shadowPass;
        //
        //std::vector<std::shared_ptr<ShadowMapPipeline>> m_shadowMapPipelines;
        //DescriptorSetGroup m_shadowMapDescGroup;

        std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;
        DescriptorSetGroup m_sceneDescSetGroup;
        std::unique_ptr<TextureView> m_sceneImageView;
    };
}
