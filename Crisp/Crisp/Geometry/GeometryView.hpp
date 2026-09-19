#pragma once

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
struct GeometryView {
    VkBuffer indexBuffer{VK_NULL_HANDLE};
    uint32_t elementCount{0};
    uint32_t instanceCount{0};
    uint32_t firstElement{0};
    int32_t vertexOffset{0};
    uint32_t firstInstance{0};

    bool isIndexed() const {
        return indexBuffer != VK_NULL_HANDLE;
    }
};
} // namespace crisp
