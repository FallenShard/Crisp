#pragma once

#include <CrispCore/Math/Headers.hpp>
#include <vulkan/vulkan.h>

namespace crisp
{
    enum class VertexAttribute
    {
        Position,
        Normal,
        TexCoord,
        Tangent,
        Custom
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
        using Type = glm::vec4;
        static constexpr std::size_t byteSize = sizeof(Type);
        static constexpr std::size_t numComponents = 4;
    };

    inline std::size_t getNumComponents(VertexAttribute attribute)
    {
        switch (attribute)
        {
            case VertexAttribute::Position:  return VertexAttributeTraits<VertexAttribute::Position>::numComponents;
            case VertexAttribute::Normal:    return VertexAttributeTraits<VertexAttribute::Normal>::numComponents;
            case VertexAttribute::TexCoord:  return VertexAttributeTraits<VertexAttribute::TexCoord>::numComponents;
            case VertexAttribute::Tangent:   return VertexAttributeTraits<VertexAttribute::Tangent>::numComponents;
            default: return 0;
        }
    }

    inline uint32_t getByteSize(VertexAttribute attribute)
    {
        switch (attribute)
        {
            case VertexAttribute::Position:  return VertexAttributeTraits<VertexAttribute::Position>::byteSize;
            case VertexAttribute::Normal:    return VertexAttributeTraits<VertexAttribute::Normal>::byteSize;
            case VertexAttribute::TexCoord:  return VertexAttributeTraits<VertexAttribute::TexCoord>::byteSize;
            case VertexAttribute::Tangent:   return VertexAttributeTraits<VertexAttribute::Tangent>::byteSize;
            default: return 0;
        }
    }

    inline VkFormat getDefaultFormat(VertexAttribute attribute)
    {
        switch (attribute)
        {
            case VertexAttribute::Position:  return VK_FORMAT_R32G32B32_SFLOAT;
            case VertexAttribute::Normal:    return VK_FORMAT_R32G32B32_SFLOAT;
            case VertexAttribute::TexCoord:  return VK_FORMAT_R32G32_SFLOAT;
            case VertexAttribute::Tangent:   return VK_FORMAT_R32G32B32A32_SFLOAT;
            default: return VkFormat();
        }
    }

    inline const char* getAttribName(VertexAttribute attribute)
    {
        switch (attribute)
        {
        case VertexAttribute::Position:  return "position";
        case VertexAttribute::Normal:    return "normal";
        case VertexAttribute::TexCoord:  return "texCoord";
        case VertexAttribute::Tangent:   return "tangent";
        default: return "";
        }
    }
}