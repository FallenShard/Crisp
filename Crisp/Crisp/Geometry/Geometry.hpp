#pragma once

#include <Crisp/Geometry/GeometryView.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>

#include <filesystem>
#include <memory>
#include <vector>

namespace crisp
{
using VertexLayoutDescription = std::vector<std::vector<VertexAttributeDescriptor>>;

class Geometry
{
public:
    Geometry() = default;
    Geometry(Renderer& renderer, const TriangleMesh& mesh, const VertexLayoutDescription& vertexLayoutDescription);
    Geometry(
        Renderer& renderer,
        const TriangleMesh& mesh,
        const VertexLayoutDescription& vertexLayoutDescription,
        bool padToVec4,
        VkBufferUsageFlags usageFlags);
    Geometry(
        Renderer& renderer,
        const VertexLayoutDescription& vertexLayoutDescription,
        std::vector<InterleavedVertexBuffer>&& vertexBuffers,
        const std::vector<glm::uvec3>& faces,
        const std::vector<TriangleMeshView>& parts = {});

    Geometry(Renderer& renderer, uint32_t vertexCount, const std::vector<glm::uvec2>& faces);

    template <typename VertexType, typename IndexType>
    Geometry(Renderer& renderer, const std::vector<VertexType>& vertices, const std::vector<IndexType>& faces)
        : m_indexCount(static_cast<uint32_t>(faces.size() * IndexType::length()))
        , m_vertexCount(static_cast<uint32_t>(vertices.size()))
        , m_instanceCount(1)
    {
        if (!vertices.empty())
        {
            auto vertexBuffer = createVertexBuffer(renderer.getDevice(), vertices.size() * sizeof(VertexType));
            renderer.fillDeviceBuffer(vertexBuffer.get(), vertices);
            m_vertexBuffers.push_back(std::move(vertexBuffer));
            m_vertexBufferHandles.push_back(m_vertexBuffers.back()->getHandle());
            m_offsets.push_back(0);
        }
        m_firstBinding = 0;
        m_bindingCount = static_cast<uint32_t>(m_vertexBufferHandles.size());

        m_indexBuffer = createIndexBuffer(renderer.getDevice(), faces.size() * sizeof(IndexType));
        renderer.fillDeviceBuffer(m_indexBuffer.get(), faces);
    }

    void addVertexBuffer(std::unique_ptr<VulkanBuffer> vertexBuffer);
    void addNonOwningVertexBuffer(VulkanBuffer* vertexBuffer);

    void bindVertexBuffers(VkCommandBuffer cmdBuffer) const;
    void bindVertexBuffers(VkCommandBuffer cmdBuffer, uint32_t firstBuffer, uint32_t bufferCount) const;
    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;
    void bindAndDraw(VkCommandBuffer commandBuffer) const;

    inline VulkanBuffer* getVertexBuffer() const
    {
        return m_vertexBuffers[0].get();
    }

    inline VulkanBuffer* getIndexBuffer() const
    {
        return m_indexBuffer.get();
    }

    inline uint32_t getIndexCount() const
    {
        return m_indexCount;
    }

    inline uint32_t getVertexCount() const
    {
        return m_vertexCount;
    }

    inline uint32_t getInstanceCount() const
    {
        return m_instanceCount;
    }

    inline void setVertexCount(uint32_t vertexCount)
    {
        m_vertexCount = vertexCount;
    }

    inline void setInstanceCount(uint32_t instanceCount)
    {
        m_instanceCount = instanceCount;
    }

    inline void setVertexBufferOffset(uint32_t bufferIndex, VkDeviceSize offset)
    {
        m_offsets.at(bufferIndex) = offset;
    }

    inline uint32_t getVertexBufferCount() const
    {
        return static_cast<uint32_t>(m_vertexBuffers.size());
    }

    inline const VertexLayout& getVertexLayout() const
    {
        return m_vertexLayout;
    }

    IndexedGeometryView createIndexedGeometryView() const;
    IndexedGeometryView createIndexedGeometryView(uint32_t partIndex) const;
    ListGeometryView createListGeometryView() const;

private:
    VertexLayout m_vertexLayout;

    // The geometry owns the buffers that contain vertex attributes.
    std::vector<std::unique_ptr<VulkanBuffer>> m_vertexBuffers;

    // This is cached for binding purposes.
    std::vector<VkBuffer> m_vertexBufferHandles;
    std::vector<VkDeviceSize> m_offsets;
    uint32_t m_firstBinding{0};
    uint32_t m_bindingCount{0};

    // The geometry also owns the index buffer if we have indexed geometry.
    std::unique_ptr<VulkanBuffer> m_indexBuffer{nullptr};

    uint32_t m_indexCount{0};
    uint32_t m_vertexCount{0};
    uint32_t m_instanceCount{0};

    std::vector<TriangleMeshView> m_parts;
};
} // namespace crisp