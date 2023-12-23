#pragma once

#include <Crisp/Geometry/GeometryView.hpp>
#include <Crisp/Geometry/VertexLayout.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>

#include <memory>
#include <vector>

namespace crisp {
class Geometry {
public:
    Geometry() = default;
    Geometry(
        Renderer& renderer,
        VulkanVertexLayout&& vertexLayout,
        const std::vector<InterleavedVertexBuffer>& interleavedVertexBuffers,
        const std::vector<glm::uvec3>& faces,
        const std::vector<TriangleMeshView>& meshViews = {},
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    template <typename VertexType, typename IndexType>
    Geometry(Renderer& renderer, const std::vector<VertexType>& vertices, const std::vector<IndexType>& faces)
        : m_vertexCount(static_cast<uint32_t>(vertices.size()))
        , m_indexCount(static_cast<uint32_t>(faces.size() * IndexType::length()))
        , m_instanceCount(1) {
        if (!vertices.empty()) {
            auto vertexBuffer = createVertexBuffer(renderer.getDevice(), vertices.size() * sizeof(VertexType));
            fillDeviceBuffer(renderer, vertexBuffer.get(), vertices);
            m_vertexBuffers.push_back(std::move(vertexBuffer));
            m_vertexBufferHandles.push_back(m_vertexBuffers.back()->getHandle());
            m_offsets.push_back(0);
        }
        m_firstBinding = 0;
        m_bindingCount = static_cast<uint32_t>(m_vertexBufferHandles.size());

        m_indexBuffer = createIndexBuffer(renderer.getDevice(), faces.size() * sizeof(IndexType));
        fillDeviceBuffer(renderer, m_indexBuffer.get(), faces);
    }

    uint32_t getIndexCount() const {
        return m_indexCount;
    }

    uint32_t getVertexCount() const {
        return m_vertexCount;
    }

    uint32_t getInstanceCount() const {
        return m_instanceCount;
    }

    void setVertexCount(uint32_t vertexCount) {
        m_vertexCount = vertexCount;
    }

    void setInstanceCount(uint32_t instanceCount) {
        m_instanceCount = instanceCount;
    }

    void addVertexBuffer(std::unique_ptr<VulkanBuffer> vertexBuffer);
    void addNonOwningVertexBuffer(VulkanBuffer* vertexBuffer);

    void bindVertexBuffers(VkCommandBuffer cmdBuffer) const;
    void bindVertexBuffers(VkCommandBuffer cmdBuffer, uint32_t firstBuffer, uint32_t bufferCount) const;
    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;
    void bindAndDraw(VkCommandBuffer commandBuffer) const;

    VulkanBuffer* getVertexBuffer(const uint32_t index = 0) const {
        return m_vertexBuffers[index].get();
    }

    VulkanBuffer* getIndexBuffer() const {
        return m_indexBuffer.get();
    }

    void setVertexBufferOffset(uint32_t bufferIndex, VkDeviceSize offset) {
        m_offsets.at(bufferIndex) = offset;
    }

    uint32_t getVertexBufferCount() const {
        return static_cast<uint32_t>(m_vertexBuffers.size());
    }

    const VulkanVertexLayout& getVertexLayout() const {
        return m_vertexLayout;
    }

    VkIndexType getIndexType() const {
        return m_indexType;
    }

    IndexedGeometryView createIndexedGeometryView() const;
    IndexedGeometryView createIndexedGeometryView(uint32_t partIndex) const;
    ListGeometryView createListGeometryView() const;

private:
    VulkanVertexLayout m_vertexLayout;

    // The geometry owns the buffers that contain vertex attributes.
    std::vector<std::unique_ptr<VulkanBuffer>> m_vertexBuffers;

    // This is cached for binding purposes.
    std::vector<VkBuffer> m_vertexBufferHandles;
    std::vector<VkDeviceSize> m_offsets;
    uint32_t m_firstBinding{0};
    uint32_t m_bindingCount{0};

    // The geometry also owns the index buffer if we have indexed geometry.
    std::unique_ptr<VulkanBuffer> m_indexBuffer{nullptr};
    VkIndexType m_indexType{VK_INDEX_TYPE_UINT32};

    uint32_t m_vertexCount{0};
    uint32_t m_indexCount{0};
    uint32_t m_instanceCount{0};

    std::vector<TriangleMeshView> m_meshViews;
};

Geometry createFromMesh(
    Renderer& renderer,
    const TriangleMesh& mesh,
    const VertexLayoutDescription& vertexLayoutDescription,
    VkBufferUsageFlags usageFlags = 0);

VkAccelerationStructureGeometryKHR createAccelerationStructureGeometry(
    const Geometry& geometry, uint64_t indexByteOffset);

} // namespace crisp