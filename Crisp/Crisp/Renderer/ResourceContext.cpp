#include <Crisp/Renderer/ResourceContext.hpp>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

namespace crisp {
ResourceContext::ResourceContext(Renderer* renderer)
    : imageCache(renderer)
    , pipelineCache(renderer->getAssetPaths())
    , m_renderer(renderer) {}

VulkanPipeline* ResourceContext::createPipeline(
    const std::string& id,
    const std::string_view filename,
    const VulkanRenderPass& renderPass,
    const uint32_t subpassIndex) {
    return pipelineCache.loadPipeline(
        id, filename, m_renderer->getShaderCache(), m_renderer->getDevice(), renderPass, subpassIndex);
}

Material* ResourceContext::createMaterial(std::string materialId, const std::string& pipelineId) {
    m_materials[materialId] = std::make_unique<Material>(pipelineCache.getPipeline(pipelineId));
    return m_materials.at(materialId).get();
}

Material* ResourceContext::createMaterial(std::string materialId, VulkanPipeline* pipeline) {
    auto* setAllocator = pipelineCache.getDescriptorAllocator(pipeline->getPipelineLayout());
    m_materials[materialId] = std::make_unique<Material>(pipeline, setAllocator);
    return m_materials.at(materialId).get();
}

Material* ResourceContext::getMaterial(const std::string_view id) const {
    return m_materials.at(id).get();
}

Geometry& ResourceContext::addGeometry(const std::string_view id, Geometry&& geometry) {
    return *m_geometries.emplace(id, std::make_unique<Geometry>(std::move(geometry))).first->second;
}

Geometry& ResourceContext::getGeometry(const std::string_view id) const {
    return *m_geometries.at(id);
}

RenderNode* ResourceContext::createPostProcessingEffectNode(
    std::string renderNodeId,
    const std::string_view pipelineFilename,
    const VulkanRenderPass& renderPass,
    const std::string& renderPassName) {
    auto renderNode = m_renderNodes.emplace(renderNodeId, std::make_unique<RenderNode>()).first->second.get();
    renderNode->geometry = m_renderer->getFullScreenGeometry();
    auto pipeline = createPipeline(renderNodeId, pipelineFilename, renderPass);
    renderNode->pass(renderPassName).pipeline = pipeline;
    renderNode->pass(renderPassName).material = createMaterial(renderNodeId, pipeline);
    return renderNode;
}

void ResourceContext::recreatePipelines() {
    pipelineCache.recreatePipelines(m_renderer->getShaderCache(), m_renderer->getDevice());
}
} // namespace crisp
