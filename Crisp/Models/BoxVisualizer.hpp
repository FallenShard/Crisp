#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Geometry/TransformPack.hpp"
#include "Geometry/Geometry.hpp"
#include "Renderer/Material.hpp"
#include "Renderer/RenderNode.hpp"

namespace crisp
{
    class CameraController;

    class Renderer;
    class UniformBuffer;
    class VertexBuffer;
    class IndexBuffer;

    class MeshGeometry;
    class VulkanPipeline;
    class VulkanRenderPass;

    class BoxVisualizer
    {
    public:
        BoxVisualizer(Renderer* renderer, uint32_t numBoxes, const VulkanRenderPass& renderPass);
        ~BoxVisualizer();

        void setBoxCorners(uint32_t i, const std::array<glm::vec3, 8>& corners);
        void setBoxColor(uint32_t i, glm::vec4 color);

        void update(const glm::mat4& V, const glm::mat4& P);

        std::vector<RenderNode> createRenderNodes() const;


    private:
        Renderer* m_renderer;

        struct BoxData
        {
            std::array<glm::vec4, 8> points;
            glm::mat4 transform;
            glm::vec4 color;
        };

        std::vector<BoxData> m_boxes;
        std::vector<glm::mat4> m_transforms;
        glm::mat4 m_VP;

        std::unique_ptr<Geometry> m_indexGeometry;
        std::unique_ptr<VulkanPipeline> m_outlinePipeline;
        std::unique_ptr<Material> m_outlineMaterial;
    };
}