#include <Crisp/Geometry/Geometry.hpp>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
namespace {
uint32_t getVertexCount(const std::vector<InterleavedVertexBuffer>& vertexBuffers) {
    if (vertexBuffers.empty()) {
        return 0;
    }

    const uint64_t vertexCount{vertexBuffers[0].buffer.size() / vertexBuffers[0].vertexSize};
    for (uint32_t i = 1; i < vertexBuffers.size(); ++i) {
        CRISP_CHECK(vertexBuffers[i].buffer.size() / vertexBuffers[i].vertexSize == vertexCount);
    }

    return static_cast<uint32_t>(vertexCount);
}

} // namespace

Geometry::Geometry(
    Renderer& renderer,
    VertexLayout&& vertexLayout,
    const std::vector<InterleavedVertexBuffer>& interleavedVertexBuffers,
    const std::vector<glm::uvec3>& faces,
    const std::vector<TriangleMeshView>& meshViews,
    const VkBufferUsageFlags usageFlags)
    : m_vertexLayout(std::move(vertexLayout))
    , m_vertexCount(::crisp::getVertexCount(interleavedVertexBuffers))
    , m_indexCount(static_cast<uint32_t>(faces.size() * 3))
    , m_instanceCount(1)
    , m_meshViews(meshViews) {
    for (const auto& buffer : interleavedVertexBuffers) {
        auto vertexBuffer = createVertexBuffer(renderer.getDevice(), buffer.buffer.size(), usageFlags);
        renderer.fillDeviceBuffer(vertexBuffer.get(), buffer.buffer);
        m_vertexBuffers.push_back(std::move(vertexBuffer));
    }

    for (const auto& buffer : m_vertexBuffers) {
        m_vertexBufferHandles.push_back(buffer->getHandle());
        m_offsets.push_back(0);
    }

    m_indexBuffer = createIndexBuffer(renderer.getDevice(), faces.size() * sizeof(glm::uvec3), usageFlags);
    renderer.fillDeviceBuffer(m_indexBuffer.get(), faces);

    m_bindingCount = static_cast<uint32_t>(m_vertexBufferHandles.size()); // NOLINT
}

void Geometry::addVertexBuffer(std::unique_ptr<VulkanBuffer> vertexBuffer) {
    m_vertexBuffers.push_back(std::move(vertexBuffer));
    m_vertexBufferHandles.push_back(m_vertexBuffers.back()->getHandle());
    m_offsets.push_back(0);
    m_bindingCount = static_cast<uint32_t>(m_vertexBufferHandles.size());
}

void Geometry::addNonOwningVertexBuffer(VulkanBuffer* vertexBuffer) {
    m_vertexBufferHandles.push_back(vertexBuffer->getHandle());
    m_offsets.push_back(0);
    m_bindingCount = static_cast<uint32_t>(m_vertexBufferHandles.size());
}

void Geometry::bindVertexBuffers(VkCommandBuffer cmdBuffer) const {
    if (m_bindingCount == 0) {
        return;
    }

    vkCmdBindVertexBuffers(cmdBuffer, m_firstBinding, m_bindingCount, m_vertexBufferHandles.data(), m_offsets.data());
}

void Geometry::bindVertexBuffers(VkCommandBuffer cmdBuffer, uint32_t firstBuffer, uint32_t bufferCount) const {
    CRISP_CHECK(firstBuffer >= m_firstBinding);
    CRISP_CHECK(firstBuffer + bufferCount <= m_firstBinding + m_bindingCount);
    if (m_bindingCount == 0) {
        return;
    }

    vkCmdBindVertexBuffers(
        cmdBuffer, firstBuffer, bufferCount, &m_vertexBufferHandles[firstBuffer], &m_offsets[firstBuffer]);
}

void Geometry::bind(VkCommandBuffer commandBuffer) const {
    if (m_bindingCount > 0) {
        vkCmdBindVertexBuffers(
            commandBuffer, m_firstBinding, m_bindingCount, m_vertexBufferHandles.data(), m_offsets.data());
    }
    if (m_indexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
    }
}

void Geometry::draw(VkCommandBuffer commandBuffer) const {
    if (m_indexBuffer) {
        vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
    }
}

void Geometry::bindAndDraw(VkCommandBuffer commandBuffer) const {
    if (m_bindingCount > 0) {
        vkCmdBindVertexBuffers(
            commandBuffer, m_firstBinding, m_bindingCount, m_vertexBufferHandles.data(), m_offsets.data());
    }

    if (m_indexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
    }
}

IndexedGeometryView Geometry::createIndexedGeometryView() const {
    return {
        .indexBuffer = m_indexBuffer->getHandle(),
        .indexCount = m_indexCount,
        .instanceCount = m_instanceCount,
        .firstIndex = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
}

IndexedGeometryView Geometry::createIndexedGeometryView(const uint32_t partIndex) const {
    return {
        .indexBuffer = m_indexBuffer->getHandle(),
        .indexCount = m_meshViews[partIndex].indexCount,
        .instanceCount = m_instanceCount,
        .firstIndex = m_meshViews[partIndex].firstIndex,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
}

ListGeometryView Geometry::createListGeometryView() const {
    return {
        .vertexCount = m_vertexCount,
        .instanceCount = m_instanceCount,
        .firstVertex = 0,
        .firstInstance = 0,
    };
}

Geometry createFromMesh(
    Renderer& renderer,
    const TriangleMesh& mesh,
    const VertexLayoutDescription& vertexLayoutDescription,
    const VkBufferUsageFlags usageFlags) {
    return {
        renderer,
        VertexLayout::create(vertexLayoutDescription),
        interleaveVertexBuffers(mesh, vertexLayoutDescription, /*padToVec4=*/false),
        mesh.getTriangles(),
        mesh.getViews(),
        usageFlags};
}

} // namespace crisp
