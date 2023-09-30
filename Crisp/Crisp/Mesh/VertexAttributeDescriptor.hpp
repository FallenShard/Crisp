#pragma once

#include <Crisp/Mesh/VertexAttributeTraits.hpp>

#include <string>
#include <vector>

namespace crisp {
struct VertexAttributeDescriptor {
    VertexAttribute type; // Enum type
    std::string name;     // String id for custom types
    uint32_t size;        // Size in bytes

    VertexAttributeDescriptor();
    VertexAttributeDescriptor(VertexAttribute attribType);
    VertexAttributeDescriptor(const std::string& name, uint32_t size);
};

template <typename T>
std::vector<T> flatten(const std::vector<std::vector<T>>& vec) {
    std::vector<T> result;
    for (const auto& v : vec) {
        result.insert(result.end(), v.begin(), v.end());
    }
    return result;
}

} // namespace crisp
