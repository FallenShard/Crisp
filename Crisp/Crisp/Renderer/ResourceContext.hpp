#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/ImageCache.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/PipelineCache.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/StorageBuffer.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>

namespace crisp
{
class ResourceContext
{
public:
    ResourceContext(Renderer* renderer);

    template <typename T>
    UniformBuffer* createUniformBuffer(std::string id, const std::vector<T>& data, BufferUpdatePolicy updatePolicy)
    {
        m_uniformBuffers[id] =
            std::make_unique<UniformBuffer>(m_renderer, data.size() * sizeof(T), updatePolicy, data.data());
        return m_uniformBuffers[id].get();
    }

    template <typename... Args>
    UniformBuffer* createUniformBuffer(const std::string& id, Args&&... args)
    {
        return addUniformBuffer(id, std::make_unique<UniformBuffer>(m_renderer, std::forward<Args>(args)...));
    }

    UniformBuffer* addUniformBuffer(std::string id, std::unique_ptr<UniformBuffer> uniformBuffer);
    UniformBuffer* getUniformBuffer(std::string id) const;

    template <typename... Args>
    StorageBuffer* createStorageBuffer(const std::string& id, Args&&... args)
    {
        return addStorageBuffer(id, std::make_unique<StorageBuffer>(m_renderer, std::forward<Args>(args)...));
    }

    StorageBuffer* addStorageBuffer(std::string id, std::unique_ptr<StorageBuffer> uniformBuffer);
    StorageBuffer* getStorageBuffer(const std::string& id) const;

    VulkanPipeline* createPipeline(
        std::string id, std::string_view luaFilename, const VulkanRenderPass& renderPass, int subpassIndex);
    Material* createMaterial(std::string materialId, std::string pipelineId);
    Material* createMaterial(std::string materialId, VulkanPipeline* pipeline);
    Material* getMaterial(std::string id) const;

    Geometry& addGeometry(std::string id, std::unique_ptr<Geometry> geometry);
    Geometry* getGeometry(std::string id) const;

    RenderNode* createPostProcessingEffectNode(
        std::string renderNodeId,
        std::string pipelineLuaFilename,
        const VulkanRenderPass& renderPass,
        const std::string& renderPassName);

    void recreatePipelines();

    inline DescriptorSetAllocator* getDescriptorAllocator(VulkanPipelineLayout* pipelineLayout)
    {
        return pipelineCache.getDescriptorAllocator(pipelineLayout);
    }

    inline const FlatHashMap<std::string, std::unique_ptr<RenderNode>>& getRenderNodes() const
    {
        return m_renderNodes;
    }

    ImageCache imageCache;
    PipelineCache pipelineCache;
    RenderTargetCache renderTargetCache;

private:
    Renderer* m_renderer;

    FlatHashMap<std::string, std::unique_ptr<Material>> m_materials;
    FlatHashMap<std::string, std::unique_ptr<Geometry>> m_geometries;
    FlatHashMap<std::string, std::unique_ptr<UniformBuffer>> m_uniformBuffers;
    FlatHashMap<std::string, std::unique_ptr<StorageBuffer>> m_storageBuffers;
    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;
};
} // namespace crisp