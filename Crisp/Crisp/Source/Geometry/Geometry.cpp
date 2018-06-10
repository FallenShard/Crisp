#include "Geometry.hpp"

#include "Geometry/TriangleMesh.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    Geometry::Geometry(Renderer* renderer, const TriangleMesh& triangleMesh)
        : Geometry(renderer, triangleMesh.interleaveAttributes(), triangleMesh.getFaces())
    {
        m_vertexCount = triangleMesh.getNumVertices();
        m_indexCount  = triangleMesh.getNumIndices();
    }

    void Geometry::bind(VkCommandBuffer commandBuffer) const
    {
        vkCmdBindVertexBuffers(commandBuffer, m_firstBinding, m_bindingCount, m_buffers.data(), m_offsets.data());
        if (m_indexBuffer)
            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
    }

    void Geometry::draw(VkCommandBuffer commandBuffer) const
    {
        if (m_indexBuffer)
            vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
        else
            vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
    }

    void Geometry::bindAndDraw(VkCommandBuffer commandBuffer) const
    {
        vkCmdBindVertexBuffers(commandBuffer, m_firstBinding, m_bindingCount, m_buffers.data(), m_offsets.data());
        if (m_indexBuffer)
        {
            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
        }
    }

    std::unique_ptr<Geometry> makeGeometry(Renderer* renderer, const std::string& filename, std::initializer_list<VertexAttribute> vertexAttributes)
    {
        return std::make_unique<Geometry>(renderer, TriangleMesh(filename, vertexAttributes));
    }

    std::unique_ptr<Geometry> makeGeometry(Renderer* renderer, TriangleMesh&& triangleMesh)
    {
        return std::make_unique<Geometry>(renderer, triangleMesh);
    }
}
