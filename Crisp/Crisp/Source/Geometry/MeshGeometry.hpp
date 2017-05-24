#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "Renderer/VertexBufferBindingGroup.hpp"

namespace crisp
{
    class VulkanDevice;
    class TriangleMesh;
    class IndexBuffer;
    class VertexBuffer;

    class MeshGeometry
    {
    public:
        MeshGeometry(VulkanRenderer* renderer, const TriangleMesh& mesh);
        ~MeshGeometry();

        void bindGeometryBuffers(VkCommandBuffer buffer) const;
        void draw(VkCommandBuffer buffer) const;

    private:
        std::unique_ptr<VertexBuffer> m_vertexBuffer;
        VertexBufferBindingGroup m_vertexBindingGroup;

        std::unique_ptr<IndexBuffer> m_indexBuffer;
        uint32_t m_numIndices;
    };
}