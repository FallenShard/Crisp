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

inline constexpr VkDeviceSize kHeapSlotStride = 64; // In bytes.

class VulkanDescriptorHeap {
public:
    enum class Type : uint8_t { Resource, Sampler };

    VulkanDescriptorHeap(VulkanDevice& device, uint32_t slotCount, Type type, std::string_view debugName);

    void encodeResource(uint32_t slot, const VkResourceDescriptorInfoEXT& resource);
    void encodeSampler(uint32_t slot, const VkSamplerCreateInfo& samplerInfo);

    struct PendingUpload {
        const VulkanBuffer& buffer;
        VkDeviceSize bufferOffset{};
        std::span<const std::byte> bytes;
    };

    std::optional<PendingUpload> takePendingUpload();

    const VkBindHeapInfoEXT& getBindInfo() const;

    uint32_t getSlotCount() const {
        return m_slotCount;
    }

private:
    void encode(uint32_t slot, VkDeviceSize descriptorSize, auto&& writeDescriptor);

    VulkanDevice* m_device;
    uint32_t m_slotCount;

    std::unique_ptr<VulkanBuffer> m_buffer;
    std::vector<std::byte> m_descriptorBytes;
    std::vector<std::byte> m_scratch;

    VkDeviceSize m_bufferOffset{0};
    VkBindHeapInfoEXT m_bindInfo{VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    bool m_uploadPending{false};
};

class VulkanResourceHeap {
public:
    VulkanResourceHeap(VulkanDevice& device, uint32_t slotCount, std::string_view debugName);

    void writeAccelerationStructure(uint32_t slot, const VulkanAccelerationStructure& accelerationStructure);
    void writeUniformBuffer(uint32_t slot, const VulkanBuffer& buffer);
    void writeStorageBuffer(uint32_t slot, const VulkanBuffer& buffer);
    void writeStorageImage(uint32_t slot, const VulkanImageView& imageView, VkImageLayout layout);
    void writeSampledImage(uint32_t slot, const VulkanImageView& imageView, VkImageLayout layout);

    VkDescriptorSetAndBindingMappingEXT makeMapping(
        uint32_t slot, uint32_t set, uint32_t binding, VkSpirvResourceTypeFlagsEXT resourceMask) const;

    VulkanDescriptorHeap& getHeap() {
        return m_heap;
    }

    const VulkanDescriptorHeap& getHeap() const {
        return m_heap;
    }

private:
    void writeBuffer(uint32_t slot, const VulkanBuffer& buffer, VkDescriptorType type);
    void writeImage(uint32_t slot, const VulkanImageView& imageView, VkImageLayout layout, VkDescriptorType type);

    VulkanDescriptorHeap m_heap;
};

class VulkanSamplerHeap {
public:
    VulkanSamplerHeap(VulkanDevice& device, uint32_t slotCount, std::string_view debugName);

    void write(uint32_t slot, const VkSamplerCreateInfo& samplerInfo);

    VulkanDescriptorHeap& getHeap() {
        return m_heap;
    }

    const VulkanDescriptorHeap& getHeap() const {
        return m_heap;
    }

private:
    VulkanDescriptorHeap m_heap;
};

} // namespace crisp
