#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Renderer/VertexBuffer.hpp"
#include "Geometry/VertexAttributeTraits.hpp"
#include "Geometry/TriangleMesh.hpp"

namespace crisp
{
    class VulkanDevice;

    class MeshGeometry
    {
    public:
        MeshGeometry(Renderer* renderer, const std::string& filename, std::initializer_list<std::initializer_list<VertexAttribute>> vertexAttributes);
        MeshGeometry(Renderer* renderer, const std::string& filename, std::initializer_list<VertexAttribute> vertexAttributes);

        template <typename T>
        MeshGeometry(Renderer* renderer, const std::vector<T>& vertices, const std::vector<glm::uvec3>& faces)
            : m_vertexBuffers(1)
            , m_firstBinding(0)
        {
            m_vertexBuffers[0] = std::make_unique<VertexBuffer>(renderer, vertices);
            m_indexBuffer      = std::make_unique<IndexBuffer>(renderer, faces);
            m_numIndices       = static_cast<uint32_t>(faces.size()) * 3;

            m_buffers.push_back(m_vertexBuffers[0]->get());
            m_offsets.push_back(0);
            m_bindingCount = static_cast<uint32_t>(m_buffers.size());
        }

        ~MeshGeometry();

        void bindGeometryBuffers(VkCommandBuffer buffer) const;
        void draw(VkCommandBuffer buffer) const;

    private:
        std::vector<std::unique_ptr<VertexBuffer>> m_vertexBuffers;
        std::unique_ptr<IndexBuffer> m_indexBuffer;
        uint32_t m_numIndices;

        std::vector<VkBuffer>     m_buffers;
        std::vector<VkDeviceSize> m_offsets;
        uint32_t m_firstBinding;
        uint32_t m_bindingCount;
    };
}