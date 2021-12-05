#pragma once

#include <Crisp/Vulkan/VulkanMemoryAllocator.hpp>

#include <CrispCore/RobinHood.hpp>

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <vector>
#include <any>

namespace crisp
{
    class VulkanPhysicalDevice;
    struct VulkanQueueConfiguration;
    class VulkanQueue;

    class VulkanDevice;

    using VulkanDestructorCallback = void(*)(void*, VulkanDevice*);
    struct DeferredDestructor
    {
        uint64_t deferredFrameIndex;
        int32_t framesRemaining;
        void* vulkanHandle;
        VulkanDestructorCallback destructorCallback;
    };

    class VulkanDevice
    {
    public:
        VulkanDevice(const VulkanPhysicalDevice& physicalDevice, const VulkanQueueConfiguration& queueConfig, int32_t virtualFrameCount);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice(VulkanDevice&&) noexcept;
        VulkanDevice& operator=(const VulkanDevice&) = delete;
        VulkanDevice& operator=(VulkanDevice&&) noexcept;

        VkDevice getHandle() const;
        const VulkanPhysicalDevice& getPhysicalDevice() const;
        const VulkanQueue* getGeneralQueue() const;
        const VulkanQueue* getTransferQueue() const;

        void invalidateMappedRange(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);
        void flushMappedRanges(VkDeviceSize nonCoherentAtomSize);

        template <typename VkHandleType>
        void deferDestruction(int32_t framesToLive, VkHandleType handle, VulkanDestructorCallback callback)
        {
            m_deferredDestructors.push_back({ m_currentFrameIndex, framesToLive, handle, callback });
        }

        void deferMemoryDeallocation(int32_t framesToLive, VulkanMemoryChunk chunk)
        {
            m_deferredDeallocations.emplace_back(framesToLive, chunk);
        }

        void updateDeferredDestructions();

        VkSemaphore createSemaphore() const;
        VkFence createFence(VkFenceCreateFlags flags) const;

        VulkanMemoryAllocator* getMemoryAllocator() const { return m_memoryAllocator.get(); }

        void postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorBufferInfo bufferInfo);
        void postDescriptorWrite(VkWriteDescriptorSet&& write, std::vector<VkDescriptorBufferInfo>&& bufferInfos);
        void postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorImageInfo imageInfo);
        void flushDescriptorUpdates();

        void setCurrentFrameIndex(uint64_t frameIndex);

        inline uint64_t getCurrentFrameIndex() const { return m_currentFrameIndex; }


        inline void addTag(void* handle, std::string tag)
        {
            m_handleTagMap.emplace(handle, std::move(tag));
        }

        inline const std::string& getTag(void* handle) const
        {
            if (const auto iter = m_handleTagMap.find(handle); iter != m_handleTagMap.end())
            {
                return iter->second;
            }
            else
            {
                return m_handleTagMap.at(VK_NULL_HANDLE);
            }
        }

        inline void removeTag(void* handle) { m_handleTagMap.erase(handle); }

    private:
        const VulkanPhysicalDevice* m_physicalDevice;
        VkDevice m_device;
        
        std::unique_ptr<VulkanQueue> m_generalQueue;
        std::unique_ptr<VulkanQueue> m_computeQueue;
        std::unique_ptr<VulkanQueue> m_transferQueue;

        std::unique_ptr<VulkanMemoryAllocator> m_memoryAllocator;
        std::vector<VkMappedMemoryRange> m_unflushedRanges;

        std::list<std::vector<VkDescriptorBufferInfo>> m_bufferInfos;
        std::list<VkDescriptorImageInfo>  m_imageInfos;
        std::vector<VkWriteDescriptorSet> m_descriptorWrites;

        std::vector<DeferredDestructor> m_deferredDestructors;
        std::vector<std::pair<int32_t, VulkanMemoryChunk>> m_deferredDeallocations;

        robin_hood::unordered_flat_map<void*, std::string> m_handleTagMap;

        uint64_t m_currentFrameIndex;
        int32_t m_virtualFrameCount;
    };
}