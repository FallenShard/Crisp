#pragma once

#include <CrispCore/Mesh/VertexAttributeTraits.hpp>

#include <string>

namespace crisp
{
    struct VertexAttributeDescriptor
    {
        VertexAttribute type; // Enum type
        std::string name;     // String id for custom types
        uint32_t size;        // Size in bytes

        VertexAttributeDescriptor();
        VertexAttributeDescriptor(VertexAttribute attribType);
        VertexAttributeDescriptor(const std::string& name, uint32_t size);
    };
}
