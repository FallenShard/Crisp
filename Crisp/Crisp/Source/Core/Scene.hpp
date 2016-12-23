#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"

namespace crisp
{
    class VulkanRenderer;
    class UniformColorPipeline;

    class Scene
    {
    public:
        Scene(VulkanRenderer* renderer);
        ~Scene();

        void update(double dt);
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
    };
}
