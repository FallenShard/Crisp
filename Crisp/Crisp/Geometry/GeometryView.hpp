#pragma once

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
struct ListGeometryView {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

struct IndexedGeometryView {
    VkBuffer indexBuffer;
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};
} // namespace crisp
