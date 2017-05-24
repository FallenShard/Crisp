#include "DynamicUniformBufferResource.hpp"

#include "Math/Headers.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "Renderer/UniformMultiBuffer.hpp"
#include "Renderer/UniformBuffer.hpp"

namespace crisp
{
    namespace gui
    {
        DynamicUniformBufferResource::DynamicUniformBufferResource(VulkanRenderer* renderer, const std::vector<VkDescriptorSet>& sets, uint32_t resourceSize, uint32_t descBinding)
            : m_renderer(renderer)
            , m_resourceSize(resourceSize)
            , m_resourcesPerGranularity(UniformBuffer::SizeGranularity / m_resourceSize)
            , m_binding(descBinding)
        {
            for (unsigned int i = 0; i < m_resourcesPerGranularity; i++)
            {
                m_idPool.insert(i); // unoccupied resource ids
            }

            // Minimum multiples of granularity to store resources
            auto multiplier = (m_idPool.size() - 1) / m_resourcesPerGranularity + 1;

            // Per-frame buffers and descriptor sets, because each command buffer in flight may hold onto its own version of data/desc set
            m_buffer = std::make_unique<UniformMultiBuffer>(renderer, UniformBuffer::SizeGranularity * multiplier, UniformBuffer::SizeGranularity);

            for (unsigned int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
            {
                // Create the descriptor set
                m_sets[i] = sets[i];
                updateSet(i);
            }
        }

        DynamicUniformBufferResource::~DynamicUniformBufferResource()
        {
        }

        uint32_t DynamicUniformBufferResource::registerResource()
        {
            auto freeId = *m_idPool.begin(); // Smallest element in a set is at .begin()
            m_idPool.erase(freeId);
            m_numRegistered++;

            // If we use up all free resource ids, resize the buffers
            if (m_idPool.empty())
            {
                for (unsigned int i = 0; i < m_resourcesPerGranularity; i++)
                {
                    m_idPool.insert(m_numRegistered + i);
                }

                auto numResources = m_numRegistered + m_idPool.size();
                auto multiplier = (numResources - 1) / m_resourcesPerGranularity + 1;

                m_buffer->resize(UniformBuffer::SizeGranularity * multiplier);
                for (auto& v : m_isSetUpdated) v = false;
            }

            return freeId;
        }

        void DynamicUniformBufferResource::updateResource(uint32_t resourceId, const void* resourceData)
        {
            m_buffer->updateStagingBuffer(resourceData, m_resourceSize, resourceId * m_resourceSize);
        }

        void DynamicUniformBufferResource::unregisterResource(uint32_t resourceId)
        {
            m_idPool.insert(resourceId);
            m_numRegistered--;
        }

        void DynamicUniformBufferResource::update(VkCommandBuffer cmdBuffer, uint32_t frameIdx)
        {
            if (!m_isSetUpdated[frameIdx])
            {
                m_buffer->resizeOnDevice(frameIdx);
                updateSet(frameIdx);
            }

            m_buffer->updateDeviceBuffer(cmdBuffer, frameIdx);
        }

        VkDescriptorSet DynamicUniformBufferResource::getDescriptorSet(uint32_t frameIdx) const
        {
            return m_sets[frameIdx];
        }

        uint32_t DynamicUniformBufferResource::getDynamicOffset(uint32_t resourceId) const
        {
            return static_cast<uint32_t>(UniformBuffer::SizeGranularity * (resourceId / m_resourcesPerGranularity));
        }

        uint32_t DynamicUniformBufferResource::getPushConstantValue(uint32_t resourceId) const
        {
            return resourceId % m_resourcesPerGranularity;
        }

        void DynamicUniformBufferResource::updateSet(uint32_t index)
        {
            VkDescriptorBufferInfo transBufferInfo = {};
            transBufferInfo.buffer = m_buffer->get(index);
            transBufferInfo.offset = 0;
            transBufferInfo.range  = UniformBuffer::SizeGranularity; // Because we use UNIFORM_BUFFER_DYNAMIC, it stays the same size

            VkWriteDescriptorSet descWrite = {};
            descWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descWrite.dstSet          = m_sets[index];
            descWrite.dstBinding      = m_binding;
            descWrite.dstArrayElement = 0;
            descWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descWrite.descriptorCount = 1;
            descWrite.pBufferInfo     = &transBufferInfo;

            vkUpdateDescriptorSets(m_renderer->getDevice()->getHandle(), 1, &descWrite, 0, nullptr);
            m_isSetUpdated[index] = true;
        }
    }
}