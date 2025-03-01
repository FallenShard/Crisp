
#include <Crisp/Renderer/PipelineCache.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/VulkanPipelineIo.hpp>
#include <Crisp/ShaderUtils/ShaderCompiler.hpp>

namespace crisp {
PipelineCache::PipelineCache(AssetPaths assetPaths)
    : m_assetPaths(std::move(assetPaths)) {}

VulkanPipeline* PipelineCache::loadPipeline(
    Renderer* renderer,
    const std::string& id,
    const std::string_view filename,
    const VulkanRenderPass& renderPass,
    const uint32_t subpassIndex) {
    auto& pipelineInfo = m_pipelineInfos[id];
    pipelineInfo.filename = filename;
    pipelineInfo.renderPass = &renderPass;
    pipelineInfo.subpassIndex = subpassIndex;

    const std::filesystem::path pipelineAbsolutePath{m_assetPaths.resourceDir / "Pipelines" / filename};
    CRISP_CHECK(exists(pipelineAbsolutePath), "Path {} doesn't exist!", pipelineAbsolutePath.string());

    auto& pipeline =
        m_pipelines
            .emplace(
                id,
                createPipelineFromFile(
                    pipelineAbsolutePath,
                    m_assetPaths.spvShaderDir,
                    renderer->getShaderCache(),
                    renderer->getDevice(),
                    renderPass,
                    subpassIndex)
                    .unwrap())
            .first->second;

    auto layout = pipeline->getPipelineLayout();
    m_descriptorAllocators[layout] = layout->createVulkanDescriptorSetAllocator(renderer->getDevice());

    return pipeline.get();
}

VulkanPipeline* PipelineCache::getPipeline(const std::string& key) const {
    return m_pipelines.at(key).get();
}

void PipelineCache::recreatePipelines(Renderer& renderer) {
    recompileShaderDir(m_assetPaths.shaderSourceDir, m_assetPaths.spvShaderDir);

    renderer.enqueueResourceUpdate([this, &renderer](VkCommandBuffer) {
        for (auto& [id, info] : m_pipelineInfos) {
            auto pipeline = renderer.createPipeline(info.filename, *info.renderPass, info.subpassIndex);
            m_pipelines[id]->swapAll(*pipeline);
        }
    });
}

} // namespace crisp
