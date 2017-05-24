#pragma once

#include <initializer_list>
#include <memory>
#include <vector>
#include <map>

#include "Geometry/TriangleMesh.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"

namespace crisp
{
    class VertexBuffer;
    class IndexBuffer;
    class VulkanDevice;

    class TriangleMeshBatch
    {
    public:
        TriangleMeshBatch(VulkanDevice* device, std::initializer_list<TriangleMesh> meshes);
        ~TriangleMeshBatch();

        std::unique_ptr<VertexBuffer> getVertexBuffer();
        std::unique_ptr<IndexBuffer> getIndexBuffer();

    private:
        VulkanDevice* m_device;
        std::map<std::string, VertexBufferBindingGroup> m_bindingGroups;

        std::unique_ptr<VertexBuffer> m_vertexBuffer;
        std::unique_ptr<IndexBuffer> m_indexBuffer;
    };
}