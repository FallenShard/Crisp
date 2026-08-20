#include <Crisp/Renderer/ResourceContext.hpp>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

namespace crisp {
ResourceContext::ResourceContext(Renderer* renderer)
    : imageCache(renderer)
    , pipelineCache(renderer->getAssetPaths(), renderer->getBindlessImageRegistry().getSetLayout())
    , m_renderer(renderer) {}

VulkanPipeline* ResourceContext::createPipeline(
    const std::string& id,
    const std::string_view filename,
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor,
    const SpecializationConstantMap& specializationConstants) {
    return pipelineCache.loadPipeline(
        id, filename, m_renderer->getDevice(), rasterizationPassDescriptor, specializationConstants);
}

Material* ResourceContext::createMaterial(std::string materialId, const std::string& pipelineId) {
    m_materials[materialId] = std::make_unique<Material>(pipelineCache.getPipeline(pipelineId));
    return m_materials.at(materialId).get();
}

Material* ResourceContext::createMaterial(
    std::string materialId, const std::string& pipelineId, const uint32_t firstSet, const uint32_t setCount) {
    m_materials[materialId] = std::make_unique<Material>(pipelineCache.getPipeline(pipelineId), firstSet, setCount);
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

void ResourceContext::recreatePipelines() {
    pipelineCache.recreatePipelines(m_renderer->getDevice());
}
} // namespace crisp
