#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

#include <Crisp/Core/Checks.hpp>

namespace crisp {

VulkanStagingBuffer::VulkanStagingBuffer(VulkanDevice& device, const VkDeviceSize capacity, const VkDeviceSize alignment)
    : m_capacity(capacity)
    , m_alignment(alignment > 0 ? alignment : 1) {
    CRISP_CHECK_GT(capacity, VkDeviceSize{0});
    m_buffer = std::make_unique<StagingVulkanBuffer>(device, capacity);
}

std::optional<StagingAllocation> VulkanStagingBuffer::allocate(const VkDeviceSize size) {
    CRISP_CHECK_GT(size, VkDeviceSize{0});

    const VkDeviceSize alignedSize = alignUp(size);

    if (m_empty) {
        // Ring is empty. Check if the allocation fits at all.
        if (alignedSize > m_capacity) {
            return std::nullopt;
        }

        // Allocate from head (which equals tail in the empty state).
        const VkDeviceSize offset = m_head;
        void* ptr = static_cast<char*>(m_buffer->getHostVisibleData<void>()) + offset; // NOLINT
        m_head = offset + alignedSize;
        m_empty = false;
        return StagingAllocation{offset, size, ptr};
    }

    if (m_head >= m_tail) {
        // Used region is [tail, head). Free space is [head, capacity) and [0, tail).
        const VkDeviceSize spaceAtEnd = m_capacity - m_head;
        if (alignedSize <= spaceAtEnd) {
            const VkDeviceSize offset = m_head;
            void* ptr = static_cast<char*>(m_buffer->getHostVisibleData<void>()) + offset; // NOLINT
            m_head = offset + alignedSize;
            return StagingAllocation{offset, size, ptr};
        }

        // Not enough space at end. Try wrapping to the beginning.
        // The tail fragment [head, capacity) is wasted.
        if (alignedSize <= m_tail) {
            m_head = alignedSize;
            void* ptr = m_buffer->getHostVisibleData<void>();
            return StagingAllocation{0, size, ptr};
        }

        // No contiguous space available.
        return std::nullopt;
    }

    // head < tail. Free space is [head, tail).
    const VkDeviceSize space = m_tail - m_head;
    if (alignedSize > space) {
        return std::nullopt;
    }

    const VkDeviceSize offset = m_head;
    void* ptr = static_cast<char*>(m_buffer->getHostVisibleData<void>()) + offset; // NOLINT
    m_head = offset + alignedSize;
    return StagingAllocation{offset, size, ptr};
}

void VulkanStagingBuffer::reclaim(const VkDeviceSize reclaimOffset) {
    m_tail = reclaimOffset;
    if (m_tail == m_head) {
        m_empty = true;
    }
}

void VulkanStagingBuffer::reclaimAll() {
    m_head = 0;
    m_tail = 0;
    m_empty = true;
}

VkBuffer VulkanStagingBuffer::getHandle() const {
    return m_buffer->getHandle();
}

VkDeviceSize VulkanStagingBuffer::getCapacity() const {
    return m_capacity;
}

VkDeviceSize VulkanStagingBuffer::getHead() const {
    return m_head;
}

VkDeviceSize VulkanStagingBuffer::getFreeBytes() const {
    if (m_empty) {
        return m_capacity;
    }

    if (m_head >= m_tail) {
        return (m_capacity - m_head) + m_tail;
    }

    return m_tail - m_head;
}

VkDeviceSize VulkanStagingBuffer::alignUp(const VkDeviceSize value) const {
    return (value + m_alignment - 1) & ~(m_alignment - 1);
}

} // namespace crisp
