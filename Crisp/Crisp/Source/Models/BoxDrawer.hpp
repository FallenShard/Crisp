#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Renderer/Transforms.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class CameraController;

    class VulkanRenderer;
    class UniformBuffer;
    class VertexBuffer;
    class IndexBuffer;

    class MeshGeometry;
    class CascadedShadowMapper;
    class OutlinePipeline;
    class VulkanRenderPass;

    class BoxDrawer
    {
    public:
        BoxDrawer(VulkanRenderer* renderer, uint32_t numBoxes, VulkanRenderPass* renderPass);
        ~BoxDrawer();

        void setBoxTransforms(const std::vector<glm::vec3>& centers, const std::vector<glm::vec3>& scales);
        void update(const glm::mat4& V, const glm::mat4& P);
        void updateDeviceBuffers(VkCommandBuffer commandBuffer, uint32_t frameIndex);

        void render(VkCommandBuffer commandBuffer, uint32_t frameIndex);

    private:
        VulkanRenderer* m_renderer;

        uint32_t m_numBoxes;

        std::unique_ptr<OutlinePipeline> m_outlinePipeline;
        DescriptorSetGroup m_outlineDesc;

        std::vector<Transforms>        m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        std::unique_ptr<VertexBuffer> m_cubeVertexBuffer;
        VertexBufferBindingGroup      m_cubeVertexBindingGroup;

        std::unique_ptr<IndexBuffer> m_indexBuffer;
        uint32_t m_numIndices;
    };
}