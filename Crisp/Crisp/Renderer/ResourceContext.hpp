#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/ImageCache.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/PipelineCache.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
class ResourceContext {
public:
    explicit ResourceContext(Renderer* renderer);

    VulkanBuffer* addBuffer(const std::string& id, std::unique_ptr<VulkanBuffer> buffer) {
        m_buffers[id] = std::move(buffer);
        return m_buffers[id].get();
    }

    VulkanBuffer* getBuffer(const std::string& id) {
        return m_buffers[id].get();
    }

    template <typename T>
    VulkanRingBuffer* createRingBufferFromStdVec(const std::string& id, const std::vector<T>& data) {
        auto buffer = std::make_unique<VulkanRingBuffer>(
            &m_renderer->getDevice(), VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, sizeof(T) * data.size(), data.data());
        m_renderer->getDevice().setObjectName(buffer->getHandle(), id.c_str());
        return m_ringBuffers.emplace(id, std::move(buffer)).first->second.get();
    }

    template <typename T>
    VulkanRingBuffer* createRingBufferFromStruct(const std::string& id, const T& data, const VkBufferUsageFlags2 usage) {
        auto buffer = std::make_unique<VulkanRingBuffer>(&m_renderer->getDevice(), usage, sizeof(T), &data);
        m_renderer->getDevice().setObjectName(buffer->getHandle(), id.c_str());
        return m_ringBuffers.emplace(id, std::move(buffer)).first->second.get();
    }

    VulkanRingBuffer* createRingBuffer(const std::string& id, const size_t size, const VkBufferUsageFlags2 usage) {
        auto buffer = std::make_unique<VulkanRingBuffer>(&m_renderer->getDevice(), usage, size);
        m_renderer->getDevice().setObjectName(buffer->getHandle(), id.c_str());
        return m_ringBuffers.emplace(id, std::move(buffer)).first->second.get();
    }

    VulkanRingBuffer* createUniformRingBuffer(
        const std::string& id, const size_t size, const VkBufferUsageFlags2 extraUsage = 0) {
        auto buffer = std::make_unique<VulkanRingBuffer>(
            &m_renderer->getDevice(), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | extraUsage, size);
        m_renderer->getDevice().setObjectName(buffer->getHandle(), id.c_str());
        return m_ringBuffers.emplace(id, std::move(buffer)).first->second.get();
    }

    VulkanRingBuffer* getRingBuffer(const std::string_view id) const {
        return m_ringBuffers.at(id).get();
    }

    template <typename T>
    VulkanBuffer* createStorageBuffer(const std::string& id, const std::vector<T>& data) {
        auto buffer = ::crisp::createStorageBuffer(m_renderer->getDevice(), data);
        m_renderer->getDevice().setObjectName(buffer->getHandle(), id.c_str());
        return m_buffers.emplace(id, std::move(buffer)).first->second.get();
    }

    VulkanPipeline* createPipeline(
        const std::string& id, std::string_view filename, const VulkanRenderPass& renderPass, uint32_t subpassIndex = 0);
    Material* createMaterial(std::string materialId, const std::string& pipelineId);
    Material* createMaterial(std::string materialId, VulkanPipeline* pipeline);
    Material* getMaterial(std::string_view id) const;

    Geometry& addGeometry(std::string_view id, Geometry&& geometry);
    Geometry& getGeometry(std::string_view id) const;

    RenderNode* createPostProcessingEffectNode(
        std::string renderNodeId,
        std::string_view pipelineFilename,
        const VulkanRenderPass& renderPass,
        const std::string& renderPassName);

    void recreatePipelines();

    VulkanDescriptorSetAllocator* getDescriptorAllocator(VulkanPipelineLayout* pipelineLayout) {
        return pipelineCache.getDescriptorAllocator(pipelineLayout);
    }

    const FlatStringHashMap<std::unique_ptr<RenderNode>>& getRenderNodes() const {
        return m_renderNodes;
    }

    ImageCache imageCache;
    PipelineCache pipelineCache;
    RenderTargetCache renderTargetCache;

private:
    Renderer* m_renderer;

    FlatStringHashMap<std::unique_ptr<Material>> m_materials;
    FlatStringHashMap<std::unique_ptr<Geometry>> m_geometries;
    FlatStringHashMap<std::unique_ptr<VulkanBuffer>> m_buffers;
    FlatStringHashMap<std::unique_ptr<VulkanRingBuffer>> m_ringBuffers;
    FlatStringHashMap<std::unique_ptr<RenderNode>> m_renderNodes;
};
} // namespace crisp