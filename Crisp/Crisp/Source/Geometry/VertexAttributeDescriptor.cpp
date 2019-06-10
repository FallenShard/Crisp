#include "Geometry/VertexAttributeDescriptor.hpp"

namespace crisp
{
    VertexAttributeDescriptor::VertexAttributeDescriptor()
        : type(VertexAttribute::Custom)
        , format(VK_FORMAT_UNDEFINED)
        , size(0)
    {
    }

    VertexAttributeDescriptor::VertexAttributeDescriptor(VertexAttribute attrib)
        : type(attrib)
        , name(getAttribName(attrib))
        , format(getDefaultFormat(attrib))
        , size(getByteSize(attrib))
    {
    }

    VertexAttributeDescriptor::VertexAttributeDescriptor(const std::string& name, VkFormat format, uint32_t size)
        : type(VertexAttribute::Custom)
        , name(name)
        , format(format)
        , size(size)
    {
    }
}