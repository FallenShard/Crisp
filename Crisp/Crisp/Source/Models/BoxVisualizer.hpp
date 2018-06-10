#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Geometry/TransformPack.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

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
        BoxVisualizer(Renderer* renderer, uint32_t numBoxes, uint32_t numFrusta, VulkanRenderPass* renderPass);
        ~BoxVisualizer();

        void update(const glm::mat4& V, const glm::mat4& P);
        void updateFrusta(CascadedShadowMapper* shadowMapper, CameraController* cameraController);

        void updateDeviceBuffers(VkCommandBuffer commandBuffer, uint32_t frameIndex);

        void render(VkCommandBuffer commandBuffer, uint32_t frameIndex);


    private:
        Renderer* m_renderer;

        uint32_t m_numFrusta;
        uint32_t m_numBoxes;

        std::unique_ptr<MeshGeometry> m_cubeGeometry;

        std::unique_ptr<VulkanPipeline> m_outlinePipeline;
        DescriptorSetGroup m_outlineDesc;

        std::vector<TransformPack> m_outlineTransforms;
        std::unique_ptr<UniformBuffer> m_outlineTransformsBuffer;

        std::vector<glm::vec3> m_frustumPoints;
        struct FrustumGeometry
        {
            std::unique_ptr<VertexBuffer> vertexBuffer;
            VertexBufferBindingGroup vertexBindingGroup;
        };
        std::vector<FrustumGeometry> m_frusta;

        std::unique_ptr<IndexBuffer> m_indexBuffer;
        uint32_t m_numIndices;

        std::unique_ptr<VertexBuffer> m_cubeVertexBuffer;
        VertexBufferBindingGroup m_cubeVertexBindingGroup;
    };
}