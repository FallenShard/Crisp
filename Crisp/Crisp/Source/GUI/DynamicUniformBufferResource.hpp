#pragma once

#include <memory>
#include <vector>
#include <set>

#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    class UniformMultiBuffer;

    namespace gui
    {
        class DynamicUniformBufferResource
        {
        public:
            DynamicUniformBufferResource(VulkanRenderer* renderer, const std::vector<VkDescriptorSet>& sets, uint32_t resourceSize, uint32_t descBinding);
            ~DynamicUniformBufferResource();

            uint32_t registerResource();
            void updateResource(uint32_t resourceId, const void* resourceData);
            void unregisterResource(uint32_t resourceId);

            void update(VkCommandBuffer cmdBuffer, uint32_t frameIdx);
            VkDescriptorSet getDescriptorSet(uint32_t frameIdx) const;

            uint32_t getDynamicOffset(uint32_t resourceId) const;
            uint32_t getPushConstantValue(uint32_t resourceId) const;

        private:
            void updateSet(uint32_t index);

            VulkanRenderer* m_renderer;

            uint32_t m_resourceSize;
            uint32_t m_resourcesPerGranularity;
            uint32_t m_binding;

            std::unique_ptr<UniformMultiBuffer>                           m_buffer;
            std::array<VkDescriptorSet, VulkanRenderer::NumVirtualFrames> m_sets;
            std::array<bool, VulkanRenderer::NumVirtualFrames>            m_isSetUpdated;

            std::set<uint32_t>                                            m_idPool;
            unsigned int                                                  m_numRegistered;
        };
    }
}