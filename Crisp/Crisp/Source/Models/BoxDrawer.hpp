#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Geometry/TransformPack.hpp"
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
    class VulkanRenderPass;
    class VulkanPipeline;

    class BoxDrawer
    {
    public:
        BoxDrawer(Renderer* renderer, uint32_t numBoxes, VulkanRenderPass* renderPass);
        ~BoxDrawer();

        void setBoxTransforms(const std::vector<glm::vec3>& centers, const std::vector<glm::vec3>& scales);
        void update(const glm::mat4& V, const glm::mat4& P);
        void updateDeviceBuffers(VkCommandBuffer commandBuffer, uint32_t frameIndex);

        void render(VkCommandBuffer commandBuffer, uint32_t frameIndex);

    private:
        Renderer* m_renderer;

        uint32_t m_numBoxes;

        std::unique_ptr<VulkanPipeline> m_outlinePipeline;
        DescriptorSetGroup m_outlineDesc;

        std::vector<TransformPack>     m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        std::unique_ptr<VertexBuffer> m_cubeVertexBuffer;
        uint32_t m_firstBinding;
        uint32_t m_bindingCount;
        std::vector<VkBuffer>     m_buffers;
        std::vector<VkDeviceSize> m_offsets;

        std::unique_ptr<IndexBuffer> m_indexBuffer;
        uint32_t m_numIndices;
    };
}