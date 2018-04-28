#include "MeshGeometry.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Geometry/TriangleMesh.hpp"

namespace crisp
{
    namespace
    {
        static void fillInterleaved(std::vector<float>& interleavedBuffer, const std::vector<float>& attribute, size_t vertexSize, size_t offset, size_t size)
        {
            for (size_t i = 0; i < attribute.size() / size; i++)
                memcpy(&interleavedBuffer[i * vertexSize + offset], &attribute[i * size], size * sizeof(float));
        }
    }

    MeshGeometry::MeshGeometry(Renderer* renderer, const std::string& filename, std::initializer_list<std::initializer_list<VertexAttribute>> vertexAttributes)
        : m_vertexBuffers(vertexAttributes.size())
    {
        TriangleMesh triangleMesh(filename);
        uint32_t numVertices = triangleMesh.getNumVertices();
        uint32_t i = 0;

        m_vertexBindingGroup.firstBinding = 0;
        m_vertexBindingGroup.bindingCount = static_cast<uint32_t>(m_vertexBuffers.size());
        for (const auto& vertexAttribFormat : vertexAttributes)
        {
            std::vector<float> interleavedBuffer;
            std::size_t interleavedPackComponents = 0;
            for (const auto& vertexAttrib : vertexAttribFormat)
                interleavedPackComponents += getNumComponents(vertexAttrib);

            interleavedBuffer.resize(interleavedPackComponents * numVertices);
            std::size_t interleaveOffset = 0;
            for (const auto& vertexAttrib : vertexAttribFormat)
            {
                auto size = getNumComponents(vertexAttrib);
                const auto& attribBuffer = triangleMesh.getAttributeBuffer(vertexAttrib);
                fillInterleaved(interleavedBuffer, attribBuffer.empty() ? std::vector<float>(numVertices * size, 0.0f) : attribBuffer, interleavedPackComponents, interleaveOffset, size);
                interleaveOffset += getNumComponents(vertexAttrib);
            }

            m_vertexBuffers[i] = std::make_unique<VertexBuffer>(renderer, interleavedBuffer);
            m_vertexBindingGroup.buffers.push_back(m_vertexBuffers[i]->get());
            m_vertexBindingGroup.offsets.push_back(0);
        }

        m_indexBuffer = std::make_unique<IndexBuffer>(renderer, triangleMesh.getFaces());
        m_numIndices = static_cast<uint32_t>(triangleMesh.getFaces().size()) * 3;
    }

    MeshGeometry::MeshGeometry(Renderer* renderer, const std::string& filename, std::initializer_list<VertexAttribute> vertexAttributes)
        : MeshGeometry(renderer, filename, { vertexAttributes })
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