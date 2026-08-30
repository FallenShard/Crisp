#include <Crisp/Vulkan/VulkanDescriptorHeap.hpp>

#include <cstring>
#include <numeric>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {
constexpr VkDeviceSize alignUp(const VkDeviceSize value, const VkDeviceSize alignment) {
    return alignment == 0 ? value : ((value + alignment - 1) / alignment) * alignment;
}

struct HeapLayout {
    VkDeviceSize alignment;
    VkDeviceSize reservedRange;
    VkDeviceSize maxSize;
    VkDeviceSize descriptorSize;
};

HeapLayout getHeapLayout(
    const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties, const VulkanDescriptorHeap::Type type) {
    if (type == VulkanDescriptorHeap::Type::Sampler) {
        return {
            .alignment = properties.samplerHeapAlignment,
            .reservedRange = properties.minSamplerHeapReservedRange,
            .maxSize = properties.maxSamplerHeapSize,
            .descriptorSize = properties.samplerDescriptorSize,
        };
    }
    return {
        .alignment = properties.resourceHeapAlignment,
        .reservedRange = properties.minResourceHeapReservedRange,
        .maxSize = properties.maxResourceHeapSize,
        .descriptorSize = std::max(properties.bufferDescriptorSize, properties.imageDescriptorSize),
    };
}

} // namespace

VulkanDescriptorHeap::VulkanDescriptorHeap(
    VulkanDevice& device, const uint32_t slotCount, const Type type, const std::string_view debugName)
    : m_device(&device)
    , m_slotCount(slotCount) {
    const auto& properties = device.getPhysicalDevice().getDescriptorHeapProperties();
    const HeapLayout layout = getHeapLayout(properties, type);

    // A uniform slot stride only addresses every descriptor type if it satisfies each type's alignment and is
    // large enough to hold it. Both are device-dependent, so they are checked rather than assumed.
    const VkDeviceSize slotAlignment = std::lcm(
        std::lcm(properties.bufferDescriptorAlignment, properties.imageDescriptorAlignment),
        properties.samplerDescriptorAlignment);
    CRISP_CHECK_EQ(kHeapSlotStride % slotAlignment, 0);
    CRISP_CHECK_LE(layout.descriptorSize, kHeapSlotStride);

    const VkDeviceSize reservedRangeOffset = alignUp(slotCount * kHeapSlotStride, slotAlignment);
    const VkDeviceSize heapSize = reservedRangeOffset + layout.reservedRange;
    CRISP_CHECK_LE(heapSize, layout.maxSize);

    m_buffer = std::make_unique<VulkanBuffer>(
        device,
        heapSize + layout.alignment - 1,
        VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
        BufferMemoryType::GpuOnly);
    device.setObjectName(*m_buffer, std::string(debugName));

    const VkDeviceAddress bufferAddress = m_buffer->getDeviceAddress();
    const VkDeviceAddress heapAddress = alignUp(bufferAddress, layout.alignment);
    m_bufferOffset = heapAddress - bufferAddress;
    m_bindInfo = {
        .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
        .heapRange = {.address = heapAddress, .size = heapSize},
        .reservedRangeOffset = reservedRangeOffset,
        .reservedRangeSize = layout.reservedRange,
    };

    m_descriptorBytes.resize(reservedRangeOffset);
    m_scratch.resize(kHeapSlotStride);
}

void VulkanDescriptorHeap::encode(const uint32_t slot, const VkDeviceSize descriptorSize, auto&& writeDescriptor) {
    CRISP_CHECK_LT(slot, m_slotCount);
    CRISP_CHECK_LE(descriptorSize, kHeapSlotStride);

    const VkHostAddressRangeEXT destination{.address = m_scratch.data(), .size = descriptorSize};
    writeDescriptor(destination);

    // Identical resource descriptions produce identical opaque bytes on one device, so this exact comparison
    // is also the dirty check for the device-local heap upload.
    std::byte* target = m_descriptorBytes.data() + (slot * kHeapSlotStride); // NOLINT
    if (std::memcmp(target, m_scratch.data(), descriptorSize) == 0) {
        return;
    }

    std::memcpy(target, m_scratch.data(), descriptorSize);
    m_uploadPending = true;
}

void VulkanDescriptorHeap::encodeResource(const uint32_t slot, const VkResourceDescriptorInfoEXT& resource) {
    const auto size = vkGetPhysicalDeviceDescriptorSizeEXT(m_device->getPhysicalDevice().getHandle(), resource.type);
    encode(slot, size, [this, &resource](const VkHostAddressRangeEXT& destination) {
        VK_FATAL(vkWriteResourceDescriptorsEXT(m_device->getHandle(), 1, &resource, &destination));
    });
}

void VulkanDescriptorHeap::encodeSampler(const uint32_t slot, const VkSamplerCreateInfo& samplerInfo) {
    const auto size =
        vkGetPhysicalDeviceDescriptorSizeEXT(m_device->getPhysicalDevice().getHandle(), VK_DESCRIPTOR_TYPE_SAMPLER);
    encode(slot, size, [this, &samplerInfo](const VkHostAddressRangeEXT& destination) {
        VK_FATAL(vkWriteSamplerDescriptorsEXT(m_device->getHandle(), 1, &samplerInfo, &destination));
    });
}

std::optional<VulkanDescriptorHeap::PendingUpload> VulkanDescriptorHeap::takePendingUpload() {
    if (!m_uploadPending) {
        return std::nullopt;
    }

    m_uploadPending = false;
    return PendingUpload{
        .buffer = *m_buffer,
        .bufferOffset = m_bufferOffset,
        .bytes = m_descriptorBytes,
    };
}

const VkBindHeapInfoEXT& VulkanDescriptorHeap::getBindInfo() const {
    return m_bindInfo;
}

VulkanResourceHeap::VulkanResourceHeap(VulkanDevice& device, const uint32_t slotCount, const std::string_view debugName)
    : m_heap(device, slotCount, VulkanDescriptorHeap::Type::Resource, debugName) {}

void VulkanResourceHeap::writeAccelerationStructure(
    const uint32_t slot, const VulkanAccelerationStructure& accelerationStructure) {
    const VkDeviceAddressRangeEXT addressRange{
        .address = accelerationStructure.getDeviceAddress(),
        .size = 0,
    };
    m_heap.encodeResource(
        slot,
        {
            .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .data = {.pAddressRange = &addressRange},
        });
}

void VulkanResourceHeap::writeBuffer(const uint32_t slot, const VulkanBuffer& buffer, const VkDescriptorType type) {
    const VkDeviceAddressRangeEXT addressRange{buffer.getDeviceAddressRange()};
    m_heap.encodeResource(
        slot,
        {
            .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
            .type = type,
            .data = {.pAddressRange = &addressRange},
        });
}

void VulkanResourceHeap::writeUniformBuffer(const uint32_t slot, const VulkanBuffer& buffer) {
    writeBuffer(slot, buffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void VulkanResourceHeap::writeStorageBuffer(const uint32_t slot, const VulkanBuffer& buffer) {
    writeBuffer(slot, buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void VulkanResourceHeap::writeImage(
    const uint32_t slot, const VulkanImageView& imageView, const VkImageLayout layout, const VkDescriptorType type) {
    const VkImageDescriptorInfoEXT imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
        .pView = &imageView.getCreateInfo(),
        .layout = layout,
    };
    m_heap.encodeResource(
        slot,
        {
            .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
            .type = type,
            .data = {.pImage = &imageInfo},
        });
}

void VulkanResourceHeap::writeStorageImage(
    const uint32_t slot, const VulkanImageView& imageView, const VkImageLayout layout) {
    writeImage(slot, imageView, layout, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}

void VulkanResourceHeap::writeSampledImage(
    const uint32_t slot, const VulkanImageView& imageView, const VkImageLayout layout) {
    writeImage(slot, imageView, layout, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
}

VkDescriptorSetAndBindingMappingEXT VulkanResourceHeap::makeMapping(
    const uint32_t slot, const uint32_t set, const uint32_t binding, const VkSpirvResourceTypeFlagsEXT resourceMask)
    const {
    CRISP_CHECK_LT(slot, m_heap.getSlotCount());
    static_assert(kHeapSlotStride <= std::numeric_limits<uint32_t>::max());

    VkDescriptorSetAndBindingMappingEXT mapping{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
        .descriptorSet = set,
        .firstBinding = binding,
        .bindingCount = 1,
        .resourceMask = resourceMask,
        .source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT,
    };
    mapping.sourceData.constantOffset.heapOffset = static_cast<uint32_t>(slot * kHeapSlotStride);
    mapping.sourceData.constantOffset.heapArrayStride = static_cast<uint32_t>(kHeapSlotStride);
    return mapping;
}

VulkanSamplerHeap::VulkanSamplerHeap(VulkanDevice& device, const uint32_t slotCount, const std::string_view debugName)
    : m_heap(device, slotCount, VulkanDescriptorHeap::Type::Sampler, debugName) {}

void VulkanSamplerHeap::write(const uint32_t slot, const VkSamplerCreateInfo& samplerInfo) {
    m_heap.encodeSampler(slot, samplerInfo);
}

} // namespace crisp
