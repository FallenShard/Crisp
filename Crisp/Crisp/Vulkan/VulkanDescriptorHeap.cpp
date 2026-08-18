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

} // namespace

VulkanDescriptorHeap::VulkanDescriptorHeap(
    VulkanDevice& device, const uint32_t slotCount, const std::string_view debugName)
    : m_device(&device)
    , m_slotCount(slotCount) {
    const auto& properties = device.getPhysicalDevice().getDescriptorHeapProperties();

    // A uniform slot stride only addresses every descriptor type if it satisfies each type's alignment and is
    // large enough to hold it. Both are device-dependent, so they are checked rather than assumed.
    const VkDeviceSize slotAlignment = std::lcm(
        std::lcm(properties.bufferDescriptorAlignment, properties.imageDescriptorAlignment),
        properties.samplerDescriptorAlignment);
    CRISP_CHECK_EQ(kHeapSlotStride % slotAlignment, 0);
    CRISP_CHECK_LE(properties.bufferDescriptorSize, kHeapSlotStride);
    CRISP_CHECK_LE(properties.imageDescriptorSize, kHeapSlotStride);

    m_reservedRangeOffset = alignUp(slotCount * kHeapSlotStride, slotAlignment);
    const VkDeviceSize heapSize = m_reservedRangeOffset + properties.minResourceHeapReservedRange;
    CRISP_CHECK_LE(heapSize, properties.maxResourceHeapSize);

    m_buffer = std::make_unique<VulkanBuffer>(
        device,
        heapSize + properties.resourceHeapAlignment - 1,
        VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
        BufferMemoryType::GpuOnly);
    device.setObjectName(*m_buffer, std::string(debugName));

    const VkDeviceAddress bufferAddress = m_buffer->getDeviceAddress();
    const VkDeviceAddress heapAddress = alignUp(bufferAddress, properties.resourceHeapAlignment);
    m_bufferOffset = heapAddress - bufferAddress;
    m_bindInfo = {
        .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
        .heapRange = {.address = heapAddress, .size = heapSize},
        .reservedRangeOffset = m_reservedRangeOffset,
        .reservedRangeSize = properties.minResourceHeapReservedRange,
    };

    m_descriptorBytes.resize(m_reservedRangeOffset);
    m_scratch.resize(kHeapSlotStride);
}

VulkanDescriptorHeap::~VulkanDescriptorHeap() = default;

void VulkanDescriptorHeap::encode(const uint32_t slot, const VkResourceDescriptorInfoEXT& resource) {
    CRISP_CHECK_LT(slot, m_slotCount);

    const auto size = static_cast<size_t>(
        vkGetPhysicalDeviceDescriptorSizeEXT(m_device->getPhysicalDevice().getHandle(), resource.type));
    CRISP_CHECK_LE(size, kHeapSlotStride);

    const VkHostAddressRangeEXT destination{.address = m_scratch.data(), .size = size};
    VK_FATAL(vkWriteResourceDescriptorsEXT(m_device->getHandle(), 1, &resource, &destination));

    // Identical resource descriptions produce identical opaque bytes on one device, so this exact comparison
    // is also the dirty check for the device-local heap upload.
    std::byte* target = m_descriptorBytes.data() + (slot * kHeapSlotStride); // NOLINT
    if (std::memcmp(target, m_scratch.data(), size) == 0) {
        return;
    }

    std::memcpy(target, m_scratch.data(), size);
    m_uploadPending = true;
}

void VulkanDescriptorHeap::writeAccelerationStructure(
    const uint32_t slot, const VulkanAccelerationStructure& accelerationStructure) {
    // The descriptor holds the structure's own address, not its backing buffer's. A non-zero size would have
    // to fall inside the range bound to that structure (VUID-VkResourceDescriptorInfoEXT-type-11484), which the
    // backing buffer's size does not describe; 0 opts out of the bound entirely.
    const VkDeviceAddressRangeEXT addressRange{
        .address = accelerationStructure.getDeviceAddress(),
        .size = 0,
    };
    encode(
        slot,
        {
            .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .data = {.pAddressRange = &addressRange},
        });
}

void VulkanDescriptorHeap::writeBuffer(const uint32_t slot, const VulkanBuffer& buffer, const VkDescriptorType type) {
    const VkDeviceAddressRangeEXT addressRange{buffer.getDeviceAddressRange()};
    encode(
        slot,
        {
            .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
            .type = type,
            .data = {.pAddressRange = &addressRange},
        });
}

void VulkanDescriptorHeap::writeUniformBuffer(const uint32_t slot, const VulkanBuffer& buffer) {
    writeBuffer(slot, buffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void VulkanDescriptorHeap::writeStorageBuffer(const uint32_t slot, const VulkanBuffer& buffer) {
    writeBuffer(slot, buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void VulkanDescriptorHeap::writeStorageImage(
    const uint32_t slot, const VulkanImageView& imageView, const VkImageLayout layout) {
    const VkImageDescriptorInfoEXT imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
        .pView = &imageView.getCreateInfo(),
        .layout = layout,
    };
    encode(
        slot,
        {
            .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .data = {.pImage = &imageInfo},
        });
}

void VulkanDescriptorHeap::uploadIfPending(
    const VulkanCommandEncoder& encoder, VulkanStagingBelt& stagingBelt, const VulkanSynchronizationStage& consumer) {
    if (!m_uploadPending) {
        return;
    }

    stagingBelt.uploadBuffer(encoder, *m_buffer, m_bufferOffset, m_descriptorBytes.data(), m_descriptorBytes.size());
    encoder.insertBufferMemoryBarrier(
        m_buffer->getHandle(), m_bufferOffset, m_reservedRangeOffset, kTransferWrite >> consumer);
    m_uploadPending = false;
}

VkDescriptorSetAndBindingMappingEXT VulkanDescriptorHeap::makeMapping(
    const uint32_t slot, const uint32_t set, const uint32_t binding, const VkSpirvResourceTypeFlagsEXT resourceMask)
    const {
    CRISP_CHECK_LT(slot, m_slotCount);
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

void VulkanDescriptorHeap::bind(const VulkanCommandEncoder& encoder) const {
    vkCmdBindResourceHeapEXT(encoder.getHandle(), &m_bindInfo);
}

} // namespace crisp
