#pragma once

#include <CrispCore/Mesh/TriangleMesh.hpp>
#include <Crisp/Geometry/GeometryView.hpp>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>

#include <filesystem>
#include <vector>
#include <memory>

namespace crisp
{
    class Geometry
    {
    public:
        Geometry(Renderer* renderer);
        Geometry(Renderer* renderer, const TriangleMesh& mesh);
        Geometry(Renderer* renderer, const TriangleMesh& mesh, const std::vector<VertexAttributeDescriptor>& vertexFormat);
        Geometry(Renderer* renderer, const TriangleMesh& mesh, const std::vector<VertexAttributeDescriptor>& vertexFormat, bool padToVec4, VkBufferUsageFlagBits usageFlags);
        Geometry(Renderer* renderer, uint32_t vertexCount, const std::vector<glm::uvec2>& faces);

        Geometry(Renderer* renderer, InterleavedVertexBuffer&& interleavedVertexBuffer, const std::vector<glm::uvec3>& faces, const std::vector<TriangleMeshView>& parts = {});

        template <typename VertexType, typename IndexType>
        Geometry(Renderer* renderer, const std::vector<VertexType>& vertices, const std::vector<IndexType>& faces)
            : m_indexBuffer(nullptr)
            , m_indexCount(0)
            , m_vertexCount(0)
            , m_instanceCount(0)
        {
            if (!vertices.empty())
            {
                auto vertexBuffer = createVertexBuffer(renderer->getDevice(), vertices.size() * sizeof(VertexType));
                renderer->fillDeviceBuffer(vertexBuffer.get(), vertices);
                m_vertexBuffers.push_back(std::move(vertexBuffer));
            }

            m_indexBuffer = createIndexBuffer(renderer->getDevice(), faces.size() * sizeof(IndexType));
            renderer->fillDeviceBuffer(m_indexBuffer.get(), faces);
            m_indexCount = static_cast<uint32_t>(faces.size()) * IndexType::length();

            for (const auto& buffer : m_vertexBuffers)
            {
                m_buffers.push_back(buffer->getHandle());
                m_offsets.push_back(0);
            }

            m_firstBinding = 0;
            m_bindingCount = static_cast<uint32_t>(m_buffers.size());

            m_instanceCount = 1;
        }

        void addVertexBuffer(std::unique_ptr<VulkanBuffer> vertexBuffer);
        void addNonOwningVertexBuffer(VulkanBuffer* vertexBuffer);

        void bindVertexBuffers(VkCommandBuffer cmdBuffer) const;
        void bind(VkCommandBuffer commandBuffer) const;
        void draw(VkCommandBuffer commandBuffer) const;
        void bindAndDraw(VkCommandBuffer commandBuffer) const;

        inline VulkanBuffer* getVertexBuffer() const { return m_vertexBuffers[0].get(); }

        inline VulkanBuffer* getIndexBuffer() const { return m_indexBuffer.get(); }
        inline uint32_t getIndexCount() const { return m_indexCount; }

        inline uint32_t getVertexCount() const { return m_vertexCount; }
        inline uint32_t getInstanceCount() const { return m_instanceCount; }

        inline void setVertexCount(uint32_t vertexCount) { m_vertexCount = vertexCount; }
        inline void setInstanceCount(uint32_t instanceCount) { m_instanceCount = instanceCount; }

        inline void setVertexBufferOffset(uint32_t bufferIndex, VkDeviceSize offset) { m_offsets.at(bufferIndex) = offset; }

        IndexedGeometryView createIndexedGeometryView() const;
        IndexedGeometryView createIndexedGeometryView(uint32_t partIndex) const;
        ListGeometryView createListGeometryView() const;

    private:
        std::vector<std::unique_ptr<VulkanBuffer>> m_vertexBuffers;
        std::unique_ptr<VulkanBuffer>              m_indexBuffer;
        uint32_t m_indexCount;
        uint32_t m_vertexCount;
        uint32_t m_instanceCount;

        std::vector<VkBuffer>     m_buffers;
        std::vector<VkDeviceSize> m_offsets;
        uint32_t m_firstBinding;
        uint32_t m_bindingCount;

        std::vector<TriangleMeshView> m_parts;
    };
}