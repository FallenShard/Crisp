#pragma once

#include <memory>
#include <set>
#include <vector>

#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {
class UniformMultiBuffer;
}

namespace crisp::gui {
class DynamicUniformBufferResource {
public:
    DynamicUniformBufferResource(
        Renderer* renderer,
        const std::array<VkDescriptorSet, kRendererVirtualFrameCount>& sets,
        uint32_t resourceSize,
        uint32_t descBinding);
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

    Renderer* m_renderer;

    uint32_t m_resourceSize;
    uint32_t m_bufferGranularity;
    uint32_t m_resourcesPerGranularity;
    uint32_t m_binding;

    std::unique_ptr<UniformMultiBuffer> m_buffer;
    std::array<VkDescriptorSet, kRendererVirtualFrameCount> m_sets;
    std::array<bool, kRendererVirtualFrameCount> m_isSetUpdated;

    std::set<uint32_t> m_idPool;
    unsigned int m_numRegistered;
};
} // namespace crisp::gui