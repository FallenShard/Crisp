#include "UniformMultiBuffer.hpp"

#include <Crisp/Renderer/Renderer.hpp>

namespace crisp
{
    UniformMultiBuffer::UniformMultiBuffer(Renderer* renderer, VkDeviceSize initialSize, VkDeviceSize /*resSize*/, const void* /*data*/)
        : m_renderer(renderer)
        , m_singleRegionSize(initialSize)
        , m_buffers(RendererConfig::VirtualFrameCount)
    {
        auto device = m_renderer->getDevice();

        for (auto& buffer : m_buffers)
        {
            buffer = std::make_unique<VulkanBuffer>(device, initialSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }

        m_stagingBuffer = std::make_unique<StagingVulkanBuffer>(device, initialSize);
    }

    UniformMultiBuffer::~UniformMultiBuffer()
    {
    }

    void UniformMultiBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        m_stagingBuffer->updateFromHost(data, size, offset);
    }

    void UniformMultiBuffer::updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex)
    {
        m_buffers[currentFrameIndex]->copyFrom(commandBuffer, *m_stagingBuffer, 0, 0, m_singleRegionSize);
    }

    uint32_t UniformMultiBuffer::getDynamicOffset(uint32_t currentFrameIndex) const
    {
        return currentFrameIndex;
    }

    void UniformMultiBuffer::resize(VkDeviceSize newSize)
    {
        auto newStagingBuffer = std::make_unique<StagingVulkanBuffer>(m_renderer->getDevice(), newSize);
        newStagingBuffer->updateFromStaging(*m_stagingBuffer);

        m_stagingBuffer = std::move(newStagingBuffer);

        m_singleRegionSize = newSize;
    }

    void UniformMultiBuffer::resizeOnDevice(uint32_t index)
    {
        auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        auto device = m_renderer->getDevice();

        m_buffers[index] = std::make_unique<VulkanBuffer>(device, m_singleRegionSize, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
}