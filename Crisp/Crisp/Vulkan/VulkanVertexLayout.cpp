#include <Crisp/Vulkan/VulkanVertexLayout.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Core/Result.hpp>

namespace crisp {
namespace {

bool operator==(const VkVertexInputBindingDescription& a, const VkVertexInputBindingDescription& b) {
    return a.binding == b.binding && a.inputRate == b.inputRate && a.stride == b.stride;
}

bool operator!=(const VkVertexInputBindingDescription& a, const VkVertexInputBindingDescription& b) {
    return !(a == b);
}

bool operator==(const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b) {
    return a.binding == b.binding && a.format == b.format && a.location == b.location && a.offset == b.offset;
}

bool operator!=(const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b) {
    return !(a == b);
}

} // namespace

bool VulkanVertexLayout::operator==(const VulkanVertexLayout& rhs) const {
    if (rhs.bindings.size() != bindings.size() || rhs.attributes.size() != attributes.size()) {
        return false;
    }

    for (uint32_t i = 0; i < bindings.size(); ++i) {
        if (bindings[i] != rhs.bindings[i]) {
            return false;
        }
    }

    for (uint32_t i = 0; i < attributes.size(); ++i) {
        if (attributes[i] != rhs.attributes[i]) {
            return false;
        }
    }
    return true;
}

bool VulkanVertexLayout::isSubsetOf(const VulkanVertexLayout& rhs) const {
    if (rhs.bindings.size() < bindings.size() || rhs.attributes.size() < attributes.size()) {
        return false;
    }

    for (uint32_t i = 0; i < bindings.size(); ++i) {
        if (bindings[i] != rhs.bindings[i]) {
            return false;
        }
    }

    for (uint32_t i = 0; i < attributes.size(); ++i) {
        if (attributes[i] != rhs.attributes[i]) {
            return false;
        }
    }
    return true;
}

} // namespace crisp
