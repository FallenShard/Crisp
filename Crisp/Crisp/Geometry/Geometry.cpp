#include <Crisp/Geometry/Geometry.hpp>

namespace crisp
{
    Geometry::Geometry(Renderer* /*renderer*/)
        : m_indexBuffer(nullptr)
        , m_indexCount(0)
        , m_vertexCount(0)
        , m_instanceCount(0)
        , m_bindingCount(0)
        , m_firstBinding(0)
    {
    }

    Geometry::Geometry(Renderer* renderer, const TriangleMesh& mesh)
        : Geometry(renderer, mesh.interleave(), mesh.getFaces(), mesh.getViews())
    {
    }

    Geometry::Geometry(Renderer* renderer, const TriangleMesh& mesh, const std::vector<VertexAttributeDescriptor>& vertexFormat)
        : Geometry(renderer, mesh.interleave(vertexFormat, false), mesh.getFaces(), mesh.getViews())
    {
    }

    Geometry::Geometry(Renderer* renderer, const TriangleMesh& mesh, const std::vector<VertexAttributeDescriptor>& vertexFormat, bool padToVec4, VkBufferUsageFlagBits usageFlags)
    {
        auto interleavedBuffer = mesh.interleave(vertexFormat, padToVec4);
        m_vertexCount = static_cast<uint32_t>(interleavedBuffer.buffer.size() / interleavedBuffer.vertexSize);
        m_indexCount = (static_cast<uint32_t>(mesh.getFaces().size() * 3));
        m_parts = mesh.getViews();

        auto vertexBuffer = createVertexBuffer(renderer->getDevice(), interleavedBuffer.buffer.size(), usageFlags);
        renderer->fillDeviceBuffer(vertexBuffer.get(), interleavedBuffer.buffer);
        m_vertexBuffers.push_back(std::move(vertexBuffer));

        for (const auto& buffer : m_vertexBuffers)
        {
            m_buffers.push_back(buffer->getHandle());
            m_offsets.push_back(0);
        }

        m_indexBuffer = createIndexBuffer(renderer->getDevice(), mesh.getFaces().size() * sizeof(glm::uvec3), usageFlags);
        renderer->fillDeviceBuffer(m_indexBuffer.get(), mesh.getFaces());


        m_firstBinding = 0;
        m_bindingCount = static_cast<uint32_t>(m_buffers.size());

        m_instanceCount = 1;
    }

    Geometry::Geometry(Renderer* renderer, uint32_t vertexCount, const std::vector<glm::uvec2>& faces)
        : m_vertexCount(vertexCount)
    {
        auto vertexBuffer = createVertexBuffer(renderer->getDevice(), Renderer::NumVirtualFrames * vertexCount * sizeof(glm::vec3));
        m_vertexBuffers.push_back(std::move(vertexBuffer));

        for (const auto& buffer : m_vertexBuffers)
        {
            m_buffers.push_back(buffer->getHandle());
            m_offsets.push_back(0);
        }

        m_indexBuffer = createIndexBuffer(renderer->getDevice(), faces.size() * sizeof(glm::uvec2));
        renderer->fillDeviceBuffer(m_indexBuffer.get(), faces);
        m_indexCount = static_cast<uint32_t>(faces.size()) * 2;

        m_firstBinding = 0;
        m_bindingCount = static_cast<uint32_t>(m_buffers.size());

        m_instanceCount = 1;
    }

    Geometry::Geometry(Renderer* renderer, InterleavedVertexBuffer&& interleavedVertexBuffer, const std::vector<glm::uvec3>& faces, const std::vector<TriangleMeshView>& parts)
        : m_vertexCount(static_cast<uint32_t>(interleavedVertexBuffer.buffer.size() / interleavedVertexBuffer.vertexSize))
        , m_indexCount(static_cast<uint32_t>(faces.size() * 3))
        , m_parts(parts)
    {
        auto vertexBuffer = createVertexBuffer(renderer->getDevice(), interleavedVertexBuffer.buffer.size());
        renderer->fillDeviceBuffer(vertexBuffer.get(), interleavedVertexBuffer.buffer);
        m_vertexBuffers.push_back(std::move(vertexBuffer));

        for (const auto& buffer : m_vertexBuffers)
        {
            m_buffers.push_back(buffer->getHandle());
            m_offsets.push_back(0);
        }

        m_indexBuffer = createIndexBuffer(renderer->getDevice(), faces.size() * sizeof(glm::uvec3));
        renderer->fillDeviceBuffer(m_indexBuffer.get(), faces);


        m_firstBinding = 0;
        m_bindingCount = static_cast<uint32_t>(m_buffers.size());

        m_instanceCount = 1;
    }

    void Geometry::addVertexBuffer(std::unique_ptr<VulkanBuffer> vertexBuffer)
    {
        m_vertexBuffers.push_back(std::move(vertexBuffer));
        m_buffers.push_back(m_vertexBuffers.back()->getHandle());
        m_offsets.push_back(0);
        m_bindingCount = static_cast<uint32_t>(m_buffers.size());
    }

    void Geometry::addNonOwningVertexBuffer(VulkanBuffer* vertexBuffer)
    {
        m_buffers.push_back(vertexBuffer->getHandle());
        m_offsets.push_back(0);
        m_bindingCount = static_cast<uint32_t>(m_buffers.size());
    }

    void Geometry::bindVertexBuffers(VkCommandBuffer cmdBuffer) const
    {
        if (m_bindingCount > 0)
            vkCmdBindVertexBuffers(cmdBuffer, m_firstBinding, m_bindingCount, m_buffers.data(), m_offsets.data());
    }

    void Geometry::bind(VkCommandBuffer commandBuffer) const
    {

        if (m_bindingCount > 0)
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
        if (m_bindingCount > 0)
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

    IndexedGeometryView Geometry::createIndexedGeometryView() const
    {
        return { m_indexBuffer->getHandle(), m_indexCount, m_instanceCount, 0, 0, 0 };
    }

    IndexedGeometryView Geometry::createIndexedGeometryView(uint32_t partIndex) const
    {
        return { m_indexBuffer->getHandle(), m_parts[partIndex].count, m_instanceCount, m_parts[partIndex].first, 0, 0 };
    }

    ListGeometryView Geometry::createListGeometryView() const
    {
        return { m_vertexCount, m_instanceCount, 0, 0 };
    }
}
