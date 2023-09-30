#include <Crisp/Mesh/VertexAttributeDescriptor.hpp>

namespace crisp {
VertexAttributeDescriptor::VertexAttributeDescriptor()
    : type(VertexAttribute::Custom)
    , size(0) {}

VertexAttributeDescriptor::VertexAttributeDescriptor(VertexAttribute attrib)
    : type(attrib)
    , name(getAttribName(attrib))
    , size(getByteSize(attrib)) {}

VertexAttributeDescriptor::VertexAttributeDescriptor(const std::string& name, uint32_t size)
    : type(VertexAttribute::Custom)
    , name(name)
    , size(size) {}
} // namespace crisp