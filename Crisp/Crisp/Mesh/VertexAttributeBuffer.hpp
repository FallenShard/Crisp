#pragma once

#include <Crisp/Mesh/VertexAttributeDescriptor.hpp>

#include <cstddef>
#include <vector>

namespace crisp
{
struct VertexAttributeBuffer
{
    VertexAttributeDescriptor descriptor;
    std::vector<std::byte> buffer;
};

struct InterleavedVertexBuffer
{
    std::size_t vertexSize = 0;
    std::vector<std::byte> buffer;
};
} // namespace crisp