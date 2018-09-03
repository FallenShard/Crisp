#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    struct ListGeometryView
    {
        uint32_t vertexCount;
        uint32_t instanceCount;
        uint32_t firstVertex;
        uint32_t firstInstance;

        ListGeometryView() {}

        ListGeometryView(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
            : vertexCount(vertexCount), instanceCount(instanceCount), firstVertex(firstVertex), firstInstance(firstInstance)
        {
        }
    };

    struct IndexedGeometryView
    {
        VkBuffer indexBuffer;
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t  vertexOffset;
        uint32_t firstInstance;

        IndexedGeometryView() {}

        IndexedGeometryView(VkBuffer indexBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
            : indexBuffer(indexBuffer), indexCount(indexCount), instanceCount(instanceCount)
            , firstIndex(firstIndex), vertexOffset(vertexOffset), firstInstance(firstInstance)
        {
        }
    };
}
