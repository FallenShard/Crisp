#pragma once

#include <Crisp/Mesh/VertexAttributeDescriptor.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace crisp {
struct VertexAttributeBuffer {
    VertexAttributeDescriptor descriptor;
    std::vector<std::byte> buffer;
};

template <typename T>
VertexAttributeBuffer createCustomVertexAttributeBuffer(std::span<const T> vertexAttributeData) {
    VertexAttributeBuffer buffer{};
    buffer.descriptor.name = "";
    buffer.descriptor.size = sizeof(T);
    buffer.descriptor.type = VertexAttribute::Custom;
    buffer.buffer.resize(vertexAttributeData.size() * buffer.descriptor.size);
    std::memcpy(buffer.buffer.data(), vertexAttributeData.data(), buffer.buffer.size());
    return buffer;
}

struct InterleavedVertexBuffer {
    std::size_t vertexSize{0};
    std::vector<std::byte> buffer;
};

} // namespace crisp