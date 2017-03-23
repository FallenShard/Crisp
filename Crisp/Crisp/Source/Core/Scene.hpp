#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"

#include "vulkan/VertexBufferBindingGroup.hpp"
#include "vulkan/DescriptorSetGroup.hpp"

namespace crisp
{
    class Application;
    class VulkanRenderer;
    class UniformColorPipeline;
    class PointSphereSpritePipeline;

    class CameraController;
    class InputDispatcher;

    class IndexBuffer;
    class UniformBuffer;

    class Skybox;

    namespace gui
    {
        class Label;
    }

    class Scene
    {
    public:
        Scene(VulkanRenderer* renderer, InputDispatcher* inputDispatcher, Application* app);
        ~Scene();

        void resize(int width, int height);

        void update(float dt);
        void render();

    private:
        VulkanRenderer* m_renderer;
        Application* m_app;
        std::unique_ptr<CameraController> m_cameraController;

        std::unique_ptr<Skybox> m_skybox;

        std::unique_ptr<UniformColorPipeline> m_pipeline;

        std::unique_ptr<PointSphereSpritePipeline> m_psPipeline;
        DescriptorSetGroup m_descriptorSetGroup;

        VertexBufferBindingGroup m_vertexBufferGroup;
        std::unique_ptr<VertexBuffer> m_buffer;

        struct Transforms
        {
            glm::mat4 MVP;
            glm::mat4 MV;
            glm::mat4 M;
            glm::mat4 N;
        };

        Transforms m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        struct ParticleParams
        {
            float radius;
            float screenSpaceScale;
        };

        ParticleParams m_params;
        std::unique_ptr<UniformBuffer> m_particleParamsBuffer;
    };
}
