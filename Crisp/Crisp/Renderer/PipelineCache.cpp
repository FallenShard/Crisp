
#include <Crisp/Renderer/PipelineCache.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Renderer/VulkanPipelineIo.hpp>
#include <Crisp/ShaderUtils/ShaderCompiler.hpp>

namespace crisp {
PipelineCache::PipelineCache(AssetPaths assetPaths)
    : m_assetPaths(std::move(assetPaths)) {}

VulkanPipeline* PipelineCache::loadPipeline(
    const std::string& id,
    const std::string_view filename,
    ShaderCache& shaderCache,
    VulkanDevice& device,
    const VulkanRenderPass& renderPass,
    const uint32_t subpassIndex) {
    auto& pipelineInfo = m_pipelineInfos[id];
    pipelineInfo.filename = filename;
    pipelineInfo.renderPass = &renderPass;
    pipelineInfo.subpassIndex = subpassIndex;

    const std::filesystem::path pipelineAbsolutePath{m_assetPaths.getPipelineConfigPath(filename)};
    CRISP_CHECK(exists(pipelineAbsolutePath), "Path {} doesn't exist!", pipelineAbsolutePath.string());

    auto& pipeline =
        m_pipelines
            .emplace(
                id,
                createPipelineFromFile(
                    pipelineAbsolutePath, m_assetPaths.spvShaderDir, shaderCache, device, renderPass, subpassIndex)
                    .unwrap())
            .first->second;

    auto layout = pipeline->getPipelineLayout();
    m_descriptorAllocators[layout] = layout->createVulkanDescriptorSetAllocator(device);

    return pipeline.get();
}

VulkanPipeline* PipelineCache::getPipeline(const std::string& key) const {
    return m_pipelines.at(key).get();
}

void PipelineCache::recreatePipelines(ShaderCache& shaderCache, const VulkanDevice& device) {
    recompileShaderDir(m_assetPaths.shaderSourceDir, m_assetPaths.spvShaderDir);

    for (auto& [id, info] : m_pipelineInfos) {
        const std::filesystem::path pipelineAbsolutePath{m_assetPaths.getPipelineConfigPath(info.filename)};
        CRISP_CHECK(exists(pipelineAbsolutePath), "Path {} doesn't exist!", pipelineAbsolutePath.string());
        auto pipeline =
            createPipelineFromFile(
                pipelineAbsolutePath, m_assetPaths.spvShaderDir, shaderCache, device, *info.renderPass, info.subpassIndex)
                .unwrap();
        m_pipelines[id]->swapAll(*pipeline);
    }
}

} // namespace crisp
