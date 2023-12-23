#pragma once

#include <Crisp/Mesh/VertexAttributeDescriptor.hpp>
#include <Crisp/Vulkan/VulkanVertexLayout.hpp>

#include <vector>

namespace crisp {
using VertexLayoutDescription = std::vector<std::vector<VertexAttributeDescriptor>>;

inline const VertexLayoutDescription kPbrVertexFormat = {
    {VertexAttribute::Position},
    {VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent},
};

inline const VertexLayoutDescription kPosVertexFormat = {
    {VertexAttribute::Position},
};

VulkanVertexLayout createVertexLayout(const VertexLayoutDescription& vertexLayoutDescription);

} // namespace crisp
