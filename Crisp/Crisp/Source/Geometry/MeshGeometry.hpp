#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Geometry/VertexAttributeTraits.hpp"
#include "Geometry/TriangleMesh.hpp"

namespace crisp
{
    class VulkanDevice;

    class MeshGeometry
    {
    public:
        MeshGeometry(VulkanRenderer* renderer, const std::string& filename, std::initializer_list<std::initializer_list<VertexAttribute>> vertexAttributes);
        MeshGeometry(VulkanRenderer* renderer, const std::string& filename, std::initializer_list<VertexAttribute> vertexAttributes);

        template <typename T>
        MeshGeometry(VulkanRenderer* renderer, const std::vector<T>& vertices, const std::vector<glm::uvec3>& faces)
            : m_vertexBuffers(1)
        {
            m_vertexBuffers[0] = std::make_unique<VertexBuffer>(renderer, vertices);
            m_vertexBindingGroup =
            {
                { m_vertexBuffers[0]->get(), 0 }
            };

            m_indexBuffer = std::make_unique<IndexBuffer>(renderer, faces);
            m_numIndices = static_cast<uint32_t>(faces.size()) * 3;
        }

        ~MeshGeometry();

        void bindGeometryBuffers(VkCommandBuffer buffer) const;
        void draw(VkCommandBuffer buffer) const;

    private:
        std::vector<std::unique_ptr<VertexBuffer>> m_vertexBuffers;
        VertexBufferBindingGroup m_vertexBindingGroup;

        std::unique_ptr<IndexBuffer> m_indexBuffer;
        uint32_t m_numIndices;
    };
}