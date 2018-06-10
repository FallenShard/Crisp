#pragma once

#include <vector>
#include <memory>

#include <vulkan/vulkan.h>

#include "Vulkan/VulkanBuffer.hpp"
#include "Renderer/Renderer.hpp"
#include "Geometry/VertexAttributeTraits.hpp"

namespace crisp
{
    class Renderer;
    class VulkanBuffer;
    class TriangleMesh;

    namespace internal
    {
        inline std::unique_ptr<VulkanBuffer> makeVertexBuffer(Renderer* renderer, VkDeviceSize size)
        {
            VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            return std::make_unique<VulkanBuffer>(renderer->getDevice(), size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }

        inline std::unique_ptr<VulkanBuffer> makeIndexBuffer(Renderer* renderer, VkDeviceSize size)
        {
            VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            return std::make_unique<VulkanBuffer>(renderer->getDevice(), size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }
    }

    class Geometry
    {
    public:
        Geometry(Renderer* renderer, const TriangleMesh& triangleMesh);

        template <typename T>
        Geometry(Renderer* renderer, const std::vector<T>& vertices, const std::vector<glm::uvec3>& faces)
            : m_indexBuffer(nullptr)
            , m_indexCount(0)
            , m_vertexCount(0)
        {
            auto vertexBuffer = internal::makeVertexBuffer(renderer, vertices.size() * sizeof(float));
            renderer->fillDeviceBuffer(vertexBuffer.get(), vertices);
            m_vertexBuffers.push_back(std::move(vertexBuffer));

            m_indexBuffer = internal::makeIndexBuffer(renderer, faces.size() * sizeof(glm::uvec3));
            renderer->fillDeviceBuffer(m_indexBuffer.get(), faces);
            m_indexCount = static_cast<uint32_t>(faces.size()) * 3;

            for (const auto& vertexBuffer : m_vertexBuffers)
            {
                m_buffers.push_back(vertexBuffer->getHandle());
                m_offsets.push_back(0);
            }

            m_firstBinding = 0;
            m_bindingCount = static_cast<uint32_t>(m_buffers.size());
        }

        void bind(VkCommandBuffer commandBuffer) const;
        void draw(VkCommandBuffer commandBuffer) const;
        void bindAndDraw(VkCommandBuffer commandBuffer) const;

    private:
        std::vector<std::unique_ptr<VulkanBuffer>> m_vertexBuffers;
        std::unique_ptr<VulkanBuffer>              m_indexBuffer;
        uint32_t m_indexCount;
        uint32_t m_vertexCount;

        std::vector<VkBuffer>     m_buffers;
        std::vector<VkDeviceSize> m_offsets;
        uint32_t m_firstBinding;
        uint32_t m_bindingCount;
    };

    std::unique_ptr<Geometry> makeGeometry(Renderer* renderer, const std::string& filename, std::initializer_list<VertexAttribute> vertexAttributes);
    std::unique_ptr<Geometry> makeGeometry(Renderer* renderer, TriangleMesh&& triangleMesh);
}