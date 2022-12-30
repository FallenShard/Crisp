#pragma once

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/ImageCache.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/PipelineCache.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>

#include <Crisp/Common/HashMap.hpp>

#include <memory>

namespace crisp
{
class Renderer;
class DescriptorSetAllocator;

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

    VulkanPipeline* createPipeline(
        std::string id, std::string_view luaFilename, const VulkanRenderPass& renderPass, int subpassIndex);
    Material* createMaterial(std::string materialId, std::string pipelineId);
    Material* createMaterial(std::string materialId, VulkanPipeline* pipeline);

    UniformBuffer* addUniformBuffer(std::string id, std::unique_ptr<UniformBuffer> uniformBuffer);

    template <typename... Args>
    UniformBuffer* createUniformBuffer(const std::string& id, Args&&... args)
    {
        return addUniformBuffer(id, std::make_unique<UniformBuffer>(m_renderer, std::forward<Args>(args)...));
    }

    Geometry& addGeometry(std::string id, std::unique_ptr<Geometry> geometry);

    RenderNode* createPostProcessingEffectNode(
        std::string renderNodeId,
        std::string pipelineLuaFilename,
        const VulkanRenderPass& renderPass,
        const std::string& renderPassName);

    Geometry* getGeometry(std::string id);
    Material* getMaterial(std::string id);
    UniformBuffer* getUniformBuffer(std::string id) const;

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
    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;
};
} // namespace crisp