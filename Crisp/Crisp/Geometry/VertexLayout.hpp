#pragma once

#include <Crisp/Mesh/VertexAttributeDescriptor.hpp>
#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <vector>

namespace crisp
{
using VertexLayoutDescription = std::vector<std::vector<VertexAttributeDescriptor>>;

struct VertexLayout
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    bool operator==(const VertexLayout& rhs) const;
    bool isSubsetOf(const VertexLayout& rhs) const;
};

VertexLayout createLayoutFromDescription(const VertexLayoutDescription& vertexLayoutDescription);

} // namespace crisp
