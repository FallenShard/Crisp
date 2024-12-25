#pragma once

#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanFormatTraits.hpp>


namespace crisp {

struct VulkanVertexLayout {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    bool operator==(const VulkanVertexLayout& rhs) const;
    bool isSubsetOf(const VulkanVertexLayout& rhs) const;

    template <VkFormat... Formats>
    void addBinding(const VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX) {
        bindings.push_back({
            .binding = static_cast<uint32_t>(bindings.size()),
            .stride = FormatSizeofValue<Formats...>,
            .inputRate = inputRate,
        });
        addAttributes<0, Formats...>(
            static_cast<uint32_t>(attributes.size()), static_cast<uint32_t>(bindings.size() - 1));
    }

    template <uint32_t Offset, VkFormat First, VkFormat... Rest>
    void addAttributes(const uint32_t location, const uint32_t binding) {
        attributes.push_back({
            .location = location,
            .binding = binding,
            .format = First,
            .offset = Offset,
        });

        if constexpr (sizeof...(Rest) > 0) {
            addAttributes<Offset + FormatSizeofValue<First>, Rest...>(static_cast<uint32_t>(attributes.size()), binding);
        }
    }
};

} // namespace crisp
