#include "ResourceContext.hpp"

#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp
{
ResourceContext::ResourceContext(Renderer* renderer)
    : m_renderer(renderer)
    , imageCache(renderer)
    , pipelineCache(renderer)
{
}

VulkanPipeline* ResourceContext::createPipeline(
    std::string id, std::string_view luaFilename, const VulkanRenderPass& renderPass, int subpassIndex)
{
    return pipelineCache.loadPipeline(id, luaFilename, renderPass, subpassIndex);
}

Material* ResourceContext::createMaterial(std::string materialId, std::string pipelineId)
{
    m_materials[materialId] = std::make_unique<Material>(pipelineCache.getPipeline(pipelineId));
    return m_materials.at(materialId).get();
}

Material* ResourceContext::createMaterial(std::string materialId, VulkanPipeline* pipeline)
{
    auto* setAllocator = pipelineCache.getDescriptorAllocator(pipeline->getPipelineLayout());
    m_materials[materialId] = std::make_unique<Material>(pipeline, setAllocator);
    return m_materials.at(materialId).get();
}

UniformBuffer* ResourceContext::addUniformBuffer(std::string id, std::unique_ptr<UniformBuffer> uniformBuffer)
{
    return m_uniformBuffers.emplace(std::move(id), std::move(uniformBuffer)).first->second.get();
}

void ResourceContext::addGeometry(std::string id, std::unique_ptr<Geometry> geometry)
{
    m_geometries[id] = std::move(geometry);
}

RenderNode* ResourceContext::createPostProcessingEffectNode(
    std::string renderNodeId,
    std::string pipelineLuaFilename,
    const VulkanRenderPass& renderPass,
    const std::string& renderPassName)
{
    auto renderNode = m_renderNodes.emplace(renderNodeId, std::make_unique<RenderNode>()).first->second.get();
    renderNode->geometry = m_renderer->getFullScreenGeometry();
    auto pipeline = createPipeline(renderNodeId, pipelineLuaFilename, renderPass, 0);
    renderNode->pass(renderPassName).pipeline = pipeline;
    renderNode->pass(renderPassName).material = createMaterial(renderNodeId, pipeline);
    return renderNode;
}

Geometry* ResourceContext::getGeometry(std::string id)
{
    return m_geometries.at(id).get();
}

Material* ResourceContext::getMaterial(std::string id)
{
    return m_materials.at(id).get();
}

UniformBuffer* ResourceContext::getUniformBuffer(std::string id)
{
    return m_uniformBuffers[id].get();
}

void ResourceContext::recreatePipelines()
{
    pipelineCache.recreatePipelines();
}
} // namespace crisp
