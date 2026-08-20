#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

namespace crisp {

// Byte stride between heap slots. Shaders addressing the heap must declare the same value as descriptor_stride
// on every unsized array, which is what makes one slot index mean one byte offset for every descriptor type.
// glslang requires the stride to be a power of two.
inline constexpr VkDeviceSize kHeapSlotStride = 64;

// A VK_EXT_descriptor_heap resource heap addressed by slot index.
//
// Shaders reach it through unsized arrays (GL_EXT_descriptor_heap), so nothing here deals in sets, bindings, or
// mappings: the array subscript in the shader is the slot written here. Slot assignment is a contract between
// the shader and its owning scene.
//
// Samplers are not handled; they live in a separate heap bound by vkCmdBindSamplerHeapEXT.
class VulkanDescriptorHeap {
public:
    VulkanDescriptorHeap(VulkanDevice& device, uint32_t slotCount, std::string_view debugName);

    VulkanDescriptorHeap(const VulkanDescriptorHeap&) = delete;
    VulkanDescriptorHeap& operator=(const VulkanDescriptorHeap&) = delete;
    VulkanDescriptorHeap(VulkanDescriptorHeap&&) = delete;
    VulkanDescriptorHeap& operator=(VulkanDescriptorHeap&&) = delete;
    ~VulkanDescriptorHeap();

    void writeAccelerationStructure(uint32_t slot, const VulkanAccelerationStructure& accelerationStructure);
    void writeUniformBuffer(uint32_t slot, const VulkanBuffer& buffer);
    void writeStorageBuffer(uint32_t slot, const VulkanBuffer& buffer);
    void writeStorageImage(uint32_t slot, const VulkanImageView& imageView, VkImageLayout layout);

    struct PendingUpload {
        const VulkanBuffer& buffer;
        VkDeviceSize bufferOffset{};
        std::span<const std::byte> bytes;
    };

    std::optional<PendingUpload> takePendingUpload();

    const VkBindHeapInfoEXT& getBindInfo() const;

    // Describes a slot to a pipeline as a legacy (set, binding) instead of a heap array subscript. Needed for
    // resources that cannot be reached through an unsized array; see docs/descriptor-heap.md.
    VkDescriptorSetAndBindingMappingEXT makeMapping(
        uint32_t slot, uint32_t set, uint32_t binding, VkSpirvResourceTypeFlagsEXT resourceMask) const;

private:
    void writeBuffer(uint32_t slot, const VulkanBuffer& buffer, VkDescriptorType type);
    void encode(uint32_t slot, const VkResourceDescriptorInfoEXT& resource);

    VulkanDevice* m_device;
    uint32_t m_slotCount;

    std::unique_ptr<VulkanBuffer> m_buffer;
    std::vector<std::byte> m_descriptorBytes;
    std::vector<std::byte> m_scratch;

    VkDeviceSize m_bufferOffset{0};
    VkDeviceSize m_reservedRangeOffset{0};
    VkBindHeapInfoEXT m_bindInfo{VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    bool m_uploadPending{false};
};

} // namespace crisp
