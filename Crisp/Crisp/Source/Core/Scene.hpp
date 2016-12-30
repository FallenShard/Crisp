#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"

namespace crisp
{
    class Application;
    class VulkanRenderer;
    class UniformColorPipeline;
    class PointSphereSpritePipeline;

    class CameraController;
    class InputDispatcher;

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

        std::unique_ptr<UniformColorPipeline> m_pipeline;
        std::vector<VkDescriptorSet> m_descSets;

        VkBuffer m_buffer;
        VkBuffer m_indexBuffer;

        VkBuffer m_colorPaletteBuffer;

        struct Transforms
        {
            glm::mat4 MVP;
            glm::mat4 MV;
            glm::mat4 M;
            glm::mat4 N;
        };

        Transforms m_transforms;
        VkBuffer m_transformsBuffer;
        VkBuffer m_transformsStagingBuffer;
        uint32_t m_updatedTransformsIndex;

        std::unique_ptr<CameraController> m_cameraController;

        Application* m_app;

        std::unique_ptr<PointSphereSpritePipeline> m_psPipeline;
        VkBuffer m_positionBuffer;
    };
}
