#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPhysicalDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace detail {
struct SampledImageTag {};

struct StorageImageTag {};
} // namespace detail

// A slot in one of the registry's descriptor arrays. Distinct types per array because the arrays alias
// view dimensionalities: reading a slot through the wrong declaration is undefined and silent, so the
// mismatch has to be a compile error.
template <typename Tag>
class BindlessImageHandle {
public:
    static constexpr uint32_t kIndexBits{24};
    static constexpr uint32_t kIndexMask{(1U << kIndexBits) - 1U};
    static constexpr uint32_t kInvalidIndex{kIndexMask};

    BindlessImageHandle() = default;

    BindlessImageHandle(const uint32_t index, const uint32_t generation)
        : m_bits((generation << kIndexBits) | (index & kIndexMask)) {}

    // The only part the GPU sees - shaders index the array with this. The generation stays on the CPU.
    uint32_t index() const {
        return m_bits & kIndexMask;
    }

    uint32_t generation() const {
        return m_bits >> kIndexBits;
    }

    bool isValid() const {
        return index() != kInvalidIndex;
    }

    bool operator==(const BindlessImageHandle&) const = default;

private:
    uint32_t m_bits{kInvalidIndex};
};

using SampledImageHandle = BindlessImageHandle<detail::SampledImageTag>;
using StorageImageHandle = BindlessImageHandle<detail::StorageImageTag>;

struct BindlessImageRegistryConfig {
    uint32_t sampledImageCapacity{16384};
    uint32_t storageImageCapacity{1024};
    uint32_t samplerCapacity{16};
};

enum class BindlessSlotState : uint8_t {
    Free,
    Live,
    Retiring, // Released, but the GPU has not yet passed the value that protects it.
};

// A Vulkan debug name can only be attached to the VulkanImageView, never to the array element holding it, so
// the slot-to-resource mapping has to be mirrored on the CPU to be inspectable at all. The name outlives the
// slot on purpose: a stale index is much easier to diagnose when the registry can say what used to be there.
struct BindlessSlotDebugInfo {
    std::string name;
    uint64_t registeredAt{0};
    uint64_t releasedAt{0};
    BindlessSlotState state{BindlessSlotState::Free};
};

// The global descriptor set every pipeline layout starts with. Allocated once, bound once per command buffer
// per bind point, never rebound - which only holds while every pipeline layout declares set 0 identically.
//
// Buffers deliberately have no array here: they travel as device addresses instead.
//
//   set 0, binding 0  SAMPLED_IMAGE [16384]   slot 0 = fallback, 1.. = registered views
//   set 0, binding 1  STORAGE_IMAGE [1024]    slot 0 = fallback, 1.. = registered views
//   set 0, binding 2  SAMPLER       [16]      append-only, no recycling
//
// A shader declares only the slice it uses. Several view types may share one binding as long as they agree on
// the descriptor type, which is what keeps every sampled image in a single index space:
//
//   layout(set = 0, binding = 0) uniform texture2D         gTextures2D[];
//   layout(set = 0, binding = 0) uniform textureCube       gTexturesCube[];
//   layout(set = 0, binding = 1, rgba8) uniform image2D    gStorageImages[];  // one array per format
//   layout(set = 0, binding = 2) uniform sampler           gSamplers[];
//
//   texture(sampler2D(gTextures2D[nonuniformEXT(mat.albedoTex)], gSamplers[mat.samplerIdx]), uv)
//
// Indexing a slot through the wrong view type is undefined, and nonuniformEXT is mandatory wherever the index
// can diverge across a wave - it is correct on NVIDIA and garbage on AMD without it.
class BindlessImageRegistry {
public:
    static constexpr uint32_t kSampledImageBinding{0};
    static constexpr uint32_t kStorageImageBinding{1};
    static constexpr uint32_t kSamplerBinding{2};

    // Slot 0 of each image array is the fallback and is never handed out.
    static constexpr uint32_t kDefaultSlot{0};

    BindlessImageRegistry(
        const VulkanDevice& device,
        const VulkanPhysicalDevice& physicalDevice,
        const BindlessImageRegistryConfig& config = {});
    ~BindlessImageRegistry();

    BindlessImageRegistry(const BindlessImageRegistry&) = delete;
    BindlessImageRegistry& operator=(const BindlessImageRegistry&) = delete;
    BindlessImageRegistry(BindlessImageRegistry&&) = delete;
    BindlessImageRegistry& operator=(BindlessImageRegistry&&) = delete;

    // Fills every slot, not just slot 0. PARTIALLY_BOUND makes reading a never-written descriptor undefined
    // rather than null, so an out-of-range or stale index has to land on something valid.
    void setDefaultSampledImage(const VulkanImageView& view);
    void setDefaultStorageImage(const VulkanImageView& view);

    SampledImageHandle addSampledImage(
        const VulkanImageView& view,
        std::string_view name,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    StorageImageHandle addStorageImage(const VulkanImageView& view, std::string_view name);

    void remove(SampledImageHandle handle);
    void remove(StorageImageHandle handle);

    // Samplers are a fixed, tiny set - no allocator, no recycling.
    uint32_t addSampler(const VulkanSampler& sampler, std::string_view name);

    // Retirement clock, mirroring VulkanResourceDeallocator. A released slot is not reusable until the GPU
    // has passed the value that was current when it was released.
    void setRetirementValue(uint64_t value);
    void collect(uint64_t completedValue);

    // One vkUpdateDescriptorSets for everything accumulated since the last call.
    void flush();

    void bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, VkPipelineBindPoint bindPoint) const;

    VkDescriptorSetLayout getSetLayout() const {
        return m_setLayout;
    }

    VkDescriptorSet getSet() const {
        return m_set;
    }

    uint32_t getSampledImageCapacity() const {
        return m_sampledImages.capacity;
    }

    uint32_t getStorageImageCapacity() const {
        return m_storageImages.capacity;
    }

    uint32_t getSamplerCount() const {
        return m_samplerCount;
    }

    size_t getPendingWriteCount() const {
        return m_pendingWrites.size();
    }

    // Indexed by slot, so [i] describes what a shader indexing i would read. Feeds the ImGui inspector.
    std::span<const BindlessSlotDebugInfo> getSampledImageSlots() const {
        return m_sampledImages.debugInfo;
    }

    std::span<const BindlessSlotDebugInfo> getStorageImageSlots() const {
        return m_storageImages.debugInfo;
    }

    std::span<const BindlessSlotDebugInfo> getSamplerSlots() const {
        return {m_samplerDebugInfo.data(), m_samplerCount};
    }

private:
    struct SlotAllocator {
        uint32_t capacity{0};
        std::vector<uint32_t> freeList; // Stack; back() is handed out next.
        std::vector<uint8_t> generations;
        std::vector<BindlessSlotDebugInfo> debugInfo;

        void initialize(uint32_t slotCapacity);
        uint32_t allocate();
    };

    struct PendingWrite {
        uint32_t binding;
        uint32_t arrayIndex;
        VkDescriptorType type;
        VkDescriptorImageInfo imageInfo;
    };

    struct PendingFree {
        uint64_t retirementValue;
        uint32_t binding;
        uint32_t index;
    };

    void createSetLayout(const BindlessImageRegistryConfig& config);
    void createPoolAndSet(const BindlessImageRegistryConfig& config);

    void queueWrite(uint32_t binding, uint32_t arrayIndex, VkDescriptorType type, const VkDescriptorImageInfo& info);
    uint32_t acquireSlot(SlotAllocator& allocator, std::string_view name);
    void releaseSlot(SlotAllocator& allocator, uint32_t binding, uint32_t index, uint32_t generation);

    const VulkanDevice* m_device{nullptr};

    VkDescriptorSetLayout m_setLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_pool{VK_NULL_HANDLE};
    VkDescriptorSet m_set{VK_NULL_HANDLE};

    SlotAllocator m_sampledImages;
    SlotAllocator m_storageImages;

    uint32_t m_samplerCapacity{0};
    uint32_t m_samplerCount{0};
    std::vector<BindlessSlotDebugInfo> m_samplerDebugInfo;

    VkDescriptorImageInfo m_defaultSampledImage{};
    VkDescriptorImageInfo m_defaultStorageImage{};

    std::vector<PendingWrite> m_pendingWrites;
    std::vector<PendingFree> m_pendingFrees;
    uint64_t m_retirementValue{0};
};
} // namespace crisp
