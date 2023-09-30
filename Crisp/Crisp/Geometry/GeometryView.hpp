#pragma once

#include <Crisp/Vulkan/VulkanHeader.hpp>

namespace crisp {
struct ListGeometryView {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;

    ListGeometryView()
        : vertexCount(0)
        , instanceCount(0)
        , firstVertex(0)
        , firstInstance(0) {}

    ListGeometryView(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
        : vertexCount(vertexCount)
        , instanceCount(instanceCount)
        , firstVertex(firstVertex)
        , firstInstance(firstInstance) {}
};

struct IndexedGeometryView {
    VkBuffer indexBuffer;
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;

    IndexedGeometryView()
        : indexBuffer(0)
        , indexCount(0)
        , instanceCount(0)
        , firstIndex(0)
        , vertexOffset(0)
        , firstInstance(0) {}

    IndexedGeometryView(
        VkBuffer indexBuffer,
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t vertexOffset,
        uint32_t firstInstance)
        : indexBuffer(indexBuffer)
        , indexCount(indexCount)
        , instanceCount(instanceCount)
        , firstIndex(firstIndex)
        , vertexOffset(vertexOffset)
        , firstInstance(firstInstance) {}
};
} // namespace crisp
