#pragma once

#include <Crisp/Math/Headers.hpp>

namespace crisp {
enum class VertexAttribute {
    Position,
    Normal,
    TexCoord,
    Tangent,
    Color,
    Weights0,
    Weights1,
    JointIndices0,
    JointIndices1,
    Custom
};

template <VertexAttribute attribute>
struct VertexAttributeTraits {
    using GlmType = typename std::tuple_element<
        static_cast<std::size_t>(attribute),
        std::tuple<glm::vec3, glm::vec3, glm::vec2, glm::vec4>>::type;
    static constexpr std::size_t byteSize = sizeof(GlmType);
    static constexpr std::size_t numComponents = GlmType::length();
};

inline constexpr std::size_t getNumComponents(VertexAttribute attribute) {
    switch (attribute) {
    case VertexAttribute::Position:
        return VertexAttributeTraits<VertexAttribute::Position>::numComponents;
    case VertexAttribute::Normal:
        return VertexAttributeTraits<VertexAttribute::Normal>::numComponents;
    case VertexAttribute::TexCoord:
        return VertexAttributeTraits<VertexAttribute::TexCoord>::numComponents;
    case VertexAttribute::Tangent:
        return VertexAttributeTraits<VertexAttribute::Tangent>::numComponents;
    default:
        return 0;
    }
}

inline constexpr uint32_t getByteSize(VertexAttribute attribute) {
    switch (attribute) {
    case VertexAttribute::Position:
        return VertexAttributeTraits<VertexAttribute::Position>::byteSize;
    case VertexAttribute::Normal:
        return VertexAttributeTraits<VertexAttribute::Normal>::byteSize;
    case VertexAttribute::TexCoord:
        return VertexAttributeTraits<VertexAttribute::TexCoord>::byteSize;
    case VertexAttribute::Tangent:
        return VertexAttributeTraits<VertexAttribute::Tangent>::byteSize;
    default:
        return 0;
    }
}

inline constexpr const char* getAttribName(VertexAttribute attribute) {
    switch (attribute) {
    case VertexAttribute::Position:
        return "position";
    case VertexAttribute::Normal:
        return "normal";
    case VertexAttribute::TexCoord:
        return "texCoord";
    case VertexAttribute::Tangent:
        return "tangent";
    default:
        return "";
    }
}
} // namespace crisp
