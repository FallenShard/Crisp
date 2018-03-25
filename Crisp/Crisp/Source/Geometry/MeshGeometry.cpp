#include "MeshGeometry.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Geometry/TriangleMesh.hpp"

namespace crisp
{
    MeshGeometry::MeshGeometry(VulkanRenderer* renderer, const TriangleMesh& mesh)
        : MeshGeometry(renderer, mesh.getBuffer(), mesh.getFaces())
    {
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