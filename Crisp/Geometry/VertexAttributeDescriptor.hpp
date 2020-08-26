#pragma once

#include "Geometry/VertexAttributeTraits.hpp"

#include <string>

#include <vulkan/vulkan.h>

namespace crisp
{
    struct VertexAttributeDescriptor
    {
        VertexAttribute type; // Enum type
        std::string name;     // String id for custom types
        VkFormat format;      // Appropriate VkFormat
        uint32_t size;        // Size in bytes

        VertexAttributeDescriptor();
        VertexAttributeDescriptor(VertexAttribute attribType);
        VertexAttributeDescriptor(const std::string& name, VkFormat format, uint32_t size);
    };
}
