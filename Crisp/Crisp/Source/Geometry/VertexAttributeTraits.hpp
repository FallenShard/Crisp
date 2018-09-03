#pragma once

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    enum class VertexAttribute
    {
        Position,
        Normal,
        TexCoord,
        Tangent,
        Bitangent
    };

    template <VertexAttribute attribute>
    struct VertexAttributeTraits;

    template <>
    struct VertexAttributeTraits<VertexAttribute::Position>
    {
        using Type = glm::vec3;
        static constexpr std::size_t byteSize = sizeof(Type);
        static constexpr std::size_t numComponents = 3;
    };

    template <>
    struct VertexAttributeTraits<VertexAttribute::Normal>
    {
        using Type = glm::vec3;
        static constexpr std::size_t byteSize = sizeof(Type);
        static constexpr std::size_t numComponents = 3;
    };

    template <>
    struct VertexAttributeTraits<VertexAttribute::TexCoord>
    {
        using Type = glm::vec2;
        static constexpr std::size_t byteSize = sizeof(Type);
        static constexpr std::size_t numComponents = 2;
    };

    template <>
    struct VertexAttributeTraits<VertexAttribute::Tangent>
    {
        using Type = glm::vec3;
        static constexpr std::size_t byteSize = sizeof(Type);
        static constexpr std::size_t numComponents = 3;
    };

    template <>
    struct VertexAttributeTraits<VertexAttribute::Bitangent>
    {
        using Type = glm::vec3;
        static constexpr std::size_t byteSize = sizeof(Type);
        static constexpr std::size_t numComponents = 3;
    };

    inline std::size_t getNumComponents(VertexAttribute attribute)
    {
        switch (attribute)
        {
            case VertexAttribute::Position:  return 3;
            case VertexAttribute::Normal:    return 3;
            case VertexAttribute::TexCoord:  return 2;
            case VertexAttribute::Tangent:   return 3;
            case VertexAttribute::Bitangent: return 3;
            default: return 0;
        }
    }

    inline std::size_t getByteSize(VertexAttribute attribute)
    {
        switch (attribute)
        {
            case VertexAttribute::Position:  return 12;
            case VertexAttribute::Normal:    return 12;
            case VertexAttribute::TexCoord:  return 8;
            case VertexAttribute::Tangent:   return 12;
            case VertexAttribute::Bitangent: return 12;
            default: return 0;
        }
    }
}