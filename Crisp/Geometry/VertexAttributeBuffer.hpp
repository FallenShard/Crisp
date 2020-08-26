#pragma once

#include <cstddef>
#include <vector>

#include <vulkan/vulkan.h>

#include "Geometry/VertexAttributeDescriptor.hpp"

namespace crisp
{
    struct VertexAttributeBuffer
    {
        VertexAttributeDescriptor descriptor;
        std::vector<std::byte> buffer;
    };

    struct InterleavedVertexBuffer
    {
        std::size_t vertexSize;
        std::vector<std::byte> buffer;
    };
}