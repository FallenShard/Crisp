#pragma once

#include <memory>

#include <gsl/pointers>

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

namespace crisp {

struct MemoryCopyRegion {
    const void* data{nullptr};
    size_t size{0};
    size_t dstOffset{0};
};

class VulkanRingBuffer {
public:
    VulkanRingBuffer(
        gsl::not_null<VulkanDevice*> device,
        VkBufferUsageFlags2 usageFlags,
        VkDeviceSize size,
        const void* data = nullptr);

    inline VkBuffer getHandle() const {
        return m_buffer->getHandle();
    }

    const VulkanBuffer& getDeviceBuffer() const {
        return *m_buffer;
    }

    void updateStagingBuffer(const MemoryCopyRegion& region, uint32_t regionIndex);

    template <typename T>
    void updateStagingBufferFromStdVec(const std::vector<T>& data, const uint32_t regionIndex) {
        updateStagingBuffer(
            {
                .data = data.data(),
                .size = sizeof(T) * data.size(),
            },
            regionIndex);
    }

    void updateDeviceBuffer(VkCommandBuffer commandBuffer);

    VkDescriptorBufferInfo getDescriptorInfo() const;
    VkDescriptorBufferInfo getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const;

private:
    VkDeviceSize m_size;
    std::unique_ptr<VulkanBuffer> m_buffer;
    std::unique_ptr<StagingVulkanBuffer> m_stagingBuffer;

    bool m_hasUpdate{false};
    uint32_t m_lastUpdatedRegion{~0u};
};

std::unique_ptr<VulkanRingBuffer> createStorageRingBuffer(
    gsl::not_null<VulkanDevice*> device, VkDeviceSize size, const void* data = nullptr);

std::unique_ptr<VulkanRingBuffer> createUniformRingBuffer(
    gsl::not_null<VulkanDevice*> device, VkDeviceSize size, const void* data = nullptr);

} // namespace crisp
