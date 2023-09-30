#include <Crisp/Renderer/PipelineCache.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/ShadingLanguage/ShaderCompiler.hpp>

namespace crisp {
PipelineCache::PipelineCache(const AssetPaths& assetPaths)
    : m_assetPaths(assetPaths) {}

VulkanPipeline* PipelineCache::loadPipeline(
    Renderer* renderer,
    const std::string& id,
    const std::string_view luaFilename,
    const VulkanRenderPass& renderPass,
    const int subpassIndex) {
    auto& pipelineInfo = m_pipelineInfos[id];
    pipelineInfo.luaFilename = luaFilename;
    pipelineInfo.renderPass = &renderPass;
    pipelineInfo.subpassIndex = subpassIndex;

    auto& pipeline =
        m_pipelines.emplace(id, renderer->createPipeline(luaFilename, renderPass, subpassIndex)).first->second;
    pipeline->setTag(id);

    auto layout = pipeline->getPipelineLayout();
    m_descriptorAllocators[layout] = layout->createDescriptorSetAllocator(renderer->getDevice());

    return pipeline.get();
}

VulkanPipeline* PipelineCache::getPipeline(const std::string& key) const {
    return m_pipelines.at(key).get();
}

void PipelineCache::recreatePipelines(Renderer& renderer) {
    recompileShaderDir(renderer.getAssetPaths().shaderSourceDir, renderer.getAssetPaths().spvShaderDir);

    renderer.enqueueResourceUpdate([this, &renderer](VkCommandBuffer) {
        for (auto& [id, info] : m_pipelineInfos) {
            auto pipeline = renderer.createPipeline(info.luaFilename, *info.renderPass, info.subpassIndex);
            m_pipelines[id]->swapAll(*pipeline);
        }
    });
}

} // namespace crisp
