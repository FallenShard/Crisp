#include "MeshGeometry.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Geometry/TriangleMesh.hpp"

namespace crisp
{
    MeshGeometry::MeshGeometry(VulkanRenderer* renderer, const TriangleMesh& mesh)
    {
        m_vertexBuffer = std::make_unique<VertexBuffer>(renderer, mesh.getBuffer());
        m_vertexBindingGroup =
        {
            { m_vertexBuffer->get(), 0 }
        };

        m_indexBuffer = std::make_unique<IndexBuffer>(renderer, mesh.getFaces());
        m_numIndices = mesh.getNumFaces() * 3;
    }

    MeshGeometry::~MeshGeometry()
    {
    }
    
    void MeshGeometry::bindGeometryBuffers(VkCommandBuffer commandBuffer) const
    {
        m_vertexBindingGroup.bind(commandBuffer);
        m_indexBuffer->bind(commandBuffer, 0);
    }

    void MeshGeometry::draw(VkCommandBuffer commandBuffer) const
    {
        vkCmdDrawIndexed(commandBuffer, m_numIndices, 1, 0, 0, 0);
    }
}