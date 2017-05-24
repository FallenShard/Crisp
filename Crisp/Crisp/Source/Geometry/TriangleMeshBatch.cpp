#include "TriangleMeshBatch.hpp"

#include <algorithm>

#include "Renderer/VertexBuffer.hpp"
#include "Renderer/IndexBuffer.hpp"

namespace crisp
{
    TriangleMeshBatch::TriangleMeshBatch(VulkanDevice* device, std::initializer_list<TriangleMesh> meshes)
    {
        size_t verticesBatchSize = 0;
        size_t indicesBatchSize = 0;
        for (auto& mesh : meshes)
        {
            verticesBatchSize += mesh.getVerticesByteSize();
            indicesBatchSize += mesh.getIndicesByteSize();
        }

        //m_vertexBuffer = std::make_unique<VertexBuffer>(device, verticesBatchSize, BufferUpdatePolicy::Constant);

        //for (auto& mesh : meshes)
        //{
        //    VertexBufferBindingGroup bindingGroup =
        //    {
        //        m_vertexBuffer.get(), 
        //    }
        //}
    }

    TriangleMeshBatch::~TriangleMeshBatch()
    {

    }

    std::unique_ptr<VertexBuffer> TriangleMeshBatch::getVertexBuffer()
    {
        return std::move(m_vertexBuffer);
    }

    std::unique_ptr<IndexBuffer> TriangleMeshBatch::getIndexBuffer()
    {
        return std::move(m_indexBuffer);
    }
}