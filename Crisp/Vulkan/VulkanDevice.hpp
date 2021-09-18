#pragma once

#include <map>
#include <set>
#include <memory>
#include <vector>
#include <functional>
#include <any>
#include <unordered_map>

#include <vulkan/vulkan.h>
#include "VulkanMemoryAllocator.hpp"

#include <spdlog/spdlog.h>

namespace crisp
{
    class VulkanContext;
    class VulkanQueue;

    class VulkanDevice;

    using VulkanDestructorCallback = void(*)(void*, VulkanDevice*);
    struct StoredDestructor
    {
        uint64_t deferredFrameIndex;
        int32_t framesRemaining;
        void* vulkanHandle;
        VulkanDestructorCallback destructorCallback;
    };

    class VulkanDevice
    {
    public:
        VulkanDevice(VulkanContext* vulkanContext, int virtualFrameCount);
        ~VulkanDevice();

        VkDevice getHandle() const;
        const VulkanQueue* getGeneralQueue() const;
        const VulkanQueue* getTransferQueue() const;
        VulkanContext* getContext() const;

        void invalidateMappedRange(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);
        void flushMappedRanges();

        template <typename VkHandleType>
        void deferDestruction(int32_t framesToLive, VkHandleType handle, VulkanDestructorCallback callback)
        {
            m_deferredDestructors.push_back({ m_currentFrameIndex, framesToLive, handle, callback });
        }

        void deferMemoryDeallocation(int32_t framesToLive, VulkanMemoryChunk chunk)
        {
            m_deferredMemoryChunks.emplace_back(framesToLive, chunk);
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
            m_handleTagMap[handle] = tag;
        }

        inline const std::string& getTag(void* handle) const { return m_handleTagMap.at(handle); }

    private:
        VulkanContext* m_context;
        int m_virtualFrameCount;
        uint64_t m_currentFrameIndex;

        VkDevice m_device;
        std::unique_ptr<VulkanQueue> m_generalQueue;
        std::unique_ptr<VulkanQueue> m_computeQueue;
        std::unique_ptr<VulkanQueue> m_transferQueue;

        std::unique_ptr<VulkanMemoryAllocator> m_memoryAllocator;

        std::vector<VkMappedMemoryRange> m_unflushedRanges;

        std::list<std::vector<VkDescriptorBufferInfo>> m_bufferInfos;
        std::list<VkDescriptorImageInfo>  m_imageInfos;
        std::vector<VkWriteDescriptorSet> m_descriptorWrites;

        std::vector<StoredDestructor> m_deferredDestructors;
        std::vector<std::pair<int32_t, VulkanMemoryChunk>> m_deferredMemoryChunks;

        std::unordered_map<void*, std::string> m_handleTagMap;
    };
}