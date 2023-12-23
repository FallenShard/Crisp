#include <Crisp/Geometry/VertexLayout.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Core/Result.hpp>

namespace crisp {
namespace {
Result<VkFormat> getFormat(const VertexAttribute attribute) {
    switch (attribute) {
    case VertexAttribute::Position:
    case VertexAttribute::Normal:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case VertexAttribute::TexCoord:
        return VK_FORMAT_R32G32_SFLOAT;
    case VertexAttribute::Tangent:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    default: {
    }
    }
    return resultError("Failed to convert attribute {} into vulkan format!", static_cast<uint32_t>(attribute));
}
} // namespace

VulkanVertexLayout createVertexLayout(const VertexLayoutDescription& vertexLayoutDescription) {
    VulkanVertexLayout layout{};
    layout.bindings.resize(vertexLayoutDescription.size(), {});
    uint32_t location{0};
    for (uint32_t i = 0; i < vertexLayoutDescription.size(); ++i) {
        const auto& desc{vertexLayoutDescription[i]};
        layout.bindings[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        layout.bindings[i].binding = i;

        uint32_t offset{0};
        for (const auto& attrib : desc) {
            layout.bindings[i].stride += attrib.size;
            layout.attributes.emplace_back(
                VkVertexInputAttributeDescription{location, i, getFormat(attrib.type).unwrap(), offset});
            offset += attrib.size;
            ++location;
        }
    }
    return layout;
}

} // namespace crisp
