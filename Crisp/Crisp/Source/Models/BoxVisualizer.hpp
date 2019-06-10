#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Geometry/TransformPack.hpp"
#include "Geometry/Geometry.hpp"
#include "Renderer/Material.hpp"
#include "Renderer/DrawCommand.hpp"

namespace crisp
{
    class CameraController;

    class Renderer;
    class UniformBuffer;
    class VertexBuffer;
    class IndexBuffer;

    class MeshGeometry;
    class CascadedShadowMapper;
    class VulkanPipeline;
    class VulkanRenderPass;

    class BoxVisualizer
    {
    public:
        BoxVisualizer(Renderer* renderer, uint32_t numBoxes, uint32_t numFrusta, const VulkanRenderPass& renderPass);
        ~BoxVisualizer();

        void updateFrusta(CascadedShadowMapper* shadowMapper, CameraController* cameraController);

        void updateDeviceBuffers(VkCommandBuffer commandBuffer, uint32_t frameIndex);

        std::vector<DrawCommand> createDrawCommands() const;


    private:
        Renderer* m_renderer;

        uint32_t m_numBoxes;

        std::vector<glm::vec3> m_frustumPoints;
        std::vector<std::unique_ptr<Geometry>> m_frusta;
        std::vector<std::unique_ptr<VulkanBuffer>> m_stagingBuffers;

        std::unique_ptr<VulkanPipeline> m_outlinePipeline;
        std::unique_ptr<Material> m_outlineMaterial;

        std::vector<TransformPack> m_outlineTransforms;
        std::unique_ptr<UniformBuffer> m_outlineTransformsBuffer;
    };
}