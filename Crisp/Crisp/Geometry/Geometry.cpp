#include <Crisp/Geometry/Geometry.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

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
    VulkanVertexLayout&& vertexLayout,
    const std::vector<InterleavedVertexBuffer>& interleavedVertexBuffers,
    const std::vector<glm::uvec3>& faces,
    const std::vector<TriangleMeshView>& meshViews,
    const VkBufferUsageFlags2 usageFlags)
    : m_vertexLayout(std::move(vertexLayout))
    , m_vertexCount(::crisp::getVertexCount(interleavedVertexBuffers))
    , m_indexCount(static_cast<uint32_t>(faces.size() * 3))
    , m_instanceCount(1)
    , m_meshViews(meshViews) {
    for (const auto& buffer : interleavedVertexBuffers) {
        m_vertexBuffers.push_back(createVertexBuffer(renderer, buffer.buffer, usageFlags));
    }

    for (const auto& buffer : m_vertexBuffers) {
        m_vertexBufferHandles.push_back(buffer->getHandle());
        m_offsets.push_back(0);
    }

    m_indexBuffer = createIndexBuffer(renderer.getDevice(), faces.size() * sizeof(glm::uvec3), usageFlags);
    fillDeviceBuffer(renderer, m_indexBuffer.get(), faces);

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

void Geometry::bindVertexBuffers(const VulkanCommandEncoder& encoder) const {
    encoder.bindVertexBuffers(m_firstBinding, m_vertexBufferHandles, m_offsets);
}

void Geometry::bindVertexBuffers(
    const VulkanCommandEncoder& encoder, const uint32_t firstBuffer, const uint32_t bufferCount) const {
    CRISP_CHECK(firstBuffer >= m_firstBinding);
    CRISP_CHECK(firstBuffer + bufferCount <= m_firstBinding + m_bindingCount);
    encoder.bindVertexBuffers(
        firstBuffer,
        std::span{m_vertexBufferHandles}.subspan(firstBuffer, bufferCount),
        std::span{m_offsets}.subspan(firstBuffer, bufferCount));
}

void Geometry::bind(const VulkanCommandEncoder& encoder) const {
    bindVertexBuffers(encoder);
    if (m_indexBuffer) {
        encoder.bindIndexBuffer(m_indexBuffer->getHandle(), 0, m_indexType);
    }
}

void Geometry::draw(const VulkanCommandEncoder& encoder) const {
    if (m_indexBuffer) {
        encoder.drawIndexed(m_indexCount, m_instanceCount);
    } else {
        encoder.draw(m_vertexCount, m_instanceCount);
    }
}

void Geometry::bindAndDraw(const VulkanCommandEncoder& encoder) const {
    bind(encoder);
    draw(encoder);
}

GeometryView Geometry::createIndexedGeometryView() const {
    return {
        .indexBuffer = m_indexBuffer->getHandle(),
        .elementCount = m_indexCount,
        .instanceCount = m_instanceCount,
        .firstElement = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
}

GeometryView Geometry::createIndexedGeometryView(const uint32_t partIndex) const {
    return {
        .indexBuffer = m_indexBuffer->getHandle(),
        .elementCount = m_meshViews[partIndex].indexCount,
        .instanceCount = m_instanceCount,
        .firstElement = m_meshViews[partIndex].firstIndex,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
}

GeometryView Geometry::createListGeometryView() const {
    return {
        .elementCount = m_vertexCount,
        .instanceCount = m_instanceCount,
        .firstElement = 0,
        .firstInstance = 0,
    };
}

Geometry createGeometry(
    Renderer& renderer,
    const TriangleMesh& mesh,
    const VertexLayoutDescription& vertexLayoutDescription,
    const VkBufferUsageFlags2 usageFlags) {
    return {
        renderer,
        createVertexLayout(vertexLayoutDescription),
        interleaveVertexBuffers(mesh, vertexLayoutDescription, /*padToVec4=*/false),
        mesh.getTriangles(),
        mesh.getViews(),
        usageFlags};
}

Geometry createMergedGeometry(
    Renderer& renderer,
    const std::span<const TriangleMesh* const> meshes,
    const VertexLayoutDescription& vertexLayoutDescription,
    const VkBufferUsageFlags2 usageFlags) {
    CRISP_CHECK(!meshes.empty());

    std::vector<InterleavedVertexBuffer> mergedVertexBuffers(vertexLayoutDescription.size());
    std::vector<glm::uvec3> mergedFaces;
    std::vector<TriangleMeshView> meshViews;
    meshViews.reserve(meshes.size());

    uint32_t vertexBase{0};
    for (const auto* mesh : meshes) {
        auto vertexBuffers = interleaveVertexBuffers(*mesh, vertexLayoutDescription, /*padToVec4=*/false);
        CRISP_CHECK_EQ(vertexBuffers.size(), mergedVertexBuffers.size());
        for (uint32_t i = 0; i < vertexBuffers.size(); ++i) {
            auto& merged = mergedVertexBuffers[i];
            const auto& part = vertexBuffers[i];
            CRISP_CHECK(merged.vertexSize == 0 || merged.vertexSize == part.vertexSize);
            merged.vertexSize = part.vertexSize;
            merged.buffer.insert(merged.buffer.end(), part.buffer.begin(), part.buffer.end());
        }

        const auto& faces = mesh->getTriangles();
        meshViews.push_back(
            {std::string{}, static_cast<uint32_t>(mergedFaces.size() * 3), static_cast<uint32_t>(faces.size() * 3)});
        for (const auto& face : faces) {
            mergedFaces.push_back(face + glm::uvec3(vertexBase));
        }
        vertexBase += mesh->getVertexCount();
    }

    return {
        renderer,
        createVertexLayout(vertexLayoutDescription),
        mergedVertexBuffers,
        mergedFaces,
        meshViews,
        usageFlags};
}

VkAccelerationStructureGeometryKHR createAccelerationStructureGeometry(
    const Geometry& geometry, const uint64_t indexByteOffset) {
    VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geo.geometry = {};
    geo.geometry.triangles = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    geo.geometry.triangles.vertexData.deviceAddress = geometry.getVertexBuffer()->getDeviceAddress();
    geo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; // Positions format.
    geo.geometry.triangles.vertexStride = sizeof(glm::vec3);          // Spacing between positions.
    geo.geometry.triangles.maxVertex = geometry.getVertexCount() - 1; // geometry.getVertexCount() - 1;
    geo.geometry.triangles.indexData.deviceAddress = geometry.getIndexBuffer()->getDeviceAddress() + indexByteOffset;
    geo.geometry.triangles.indexType = geometry.getIndexType();
    return geo;
}

} // namespace crisp
