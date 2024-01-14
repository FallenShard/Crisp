#include <Crisp/Gui/DynamicUniformBufferResource.hpp>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/UniformMultiBuffer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp::gui {
DynamicUniformBufferResource::DynamicUniformBufferResource(
    Renderer* renderer,
    const std::array<VkDescriptorSet, kRendererVirtualFrameCount>& sets,
    uint32_t resourceSize,
    uint32_t descBinding)
    : m_renderer(renderer)
    , m_resourceSize(resourceSize)
    , m_bufferGranularity(
          static_cast<uint32_t>(renderer->getPhysicalDevice().getLimits().minUniformBufferOffsetAlignment))
    , m_resourcesPerGranularity(m_bufferGranularity / m_resourceSize)
    , m_binding(descBinding)
    , m_sets(sets)
    , m_numRegistered(0) {

    for (unsigned int i = 0; i < m_resourcesPerGranularity; i++) {
        m_idPool.insert(i); // unoccupied resource ids
    }

    // Minimum multiples of granularity to store resources
    auto multiplier = (m_idPool.size() - 1) / m_resourcesPerGranularity + 1;

    // Per-frame buffers and descriptor sets, because each command buffer in flight may hold onto its own version of
    // data/desc set
    m_buffer = std::make_unique<UniformMultiBuffer>(renderer, m_bufferGranularity * multiplier, m_bufferGranularity);

    for (unsigned int i = 0; i < kRendererVirtualFrameCount; i++) {
        updateSet(i);
    }
}

DynamicUniformBufferResource::~DynamicUniformBufferResource() {}

uint32_t DynamicUniformBufferResource::registerResource() {
    auto freeId = *m_idPool.begin(); // Smallest element in a set is at .begin()
    m_idPool.erase(m_idPool.begin());
    m_numRegistered++;

    // If we use up all free resource ids, resize the buffers
    if (m_idPool.empty()) {
        for (unsigned int i = 0; i < m_resourcesPerGranularity; i++) {
            m_idPool.insert(m_numRegistered + i);
        }

        auto numResources = m_numRegistered + m_idPool.size();
        auto multiplier = (numResources - 1) / m_resourcesPerGranularity + 1;

        m_buffer->resize(m_bufferGranularity * multiplier);
        for (auto& v : m_isSetUpdated) {
            v = false;
        }
    }

    return freeId;
}

void DynamicUniformBufferResource::updateResource(uint32_t resourceId, const void* resourceData) {
    m_buffer->updateStagingBuffer(resourceData, m_resourceSize, resourceId * m_resourceSize);
}

void DynamicUniformBufferResource::unregisterResource(uint32_t resourceId) {
    m_idPool.insert(resourceId);
    m_numRegistered--;
}

void DynamicUniformBufferResource::update(VkCommandBuffer cmdBuffer, uint32_t frameIdx) {
    if (!m_isSetUpdated[frameIdx]) {
        m_buffer->resizeOnDevice(frameIdx);
        updateSet(frameIdx);
    }

    m_buffer->updateDeviceBuffer(cmdBuffer, frameIdx);
}

VkDescriptorSet DynamicUniformBufferResource::getDescriptorSet(uint32_t frameIdx) const {
    return m_sets[frameIdx];
}

uint32_t DynamicUniformBufferResource::getDynamicOffset(uint32_t resourceId) const {
    return static_cast<uint32_t>(m_bufferGranularity * (resourceId / m_resourcesPerGranularity));
}

uint32_t DynamicUniformBufferResource::getPushConstantValue(uint32_t resourceId) const {
    return resourceId % m_resourcesPerGranularity;
}

void DynamicUniformBufferResource::updateSet(uint32_t index) {
    VkDescriptorBufferInfo transBufferInfo = {};
    transBufferInfo.buffer = m_buffer->get(index);
    transBufferInfo.offset = 0;
    transBufferInfo.range = m_bufferGranularity;

    VkWriteDescriptorSet descWrite = {};
    descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrite.dstSet = m_sets[index];
    descWrite.dstBinding = m_binding;
    descWrite.dstArrayElement = 0;
    descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descWrite.descriptorCount = 1;
    descWrite.pBufferInfo = &transBufferInfo;

    vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), 1, &descWrite, 0, nullptr);
    m_isSetUpdated[index] = true;
}
} // namespace crisp::gui