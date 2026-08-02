
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
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor) {
    PipelineInfo pipelineInfo{
        .filename = std::string(filename), .rasterizationPassDescriptor = rasterizationPassDescriptor};

    auto& storedPipelineInfo = m_pipelineInfos[id];
    storedPipelineInfo = std::move(pipelineInfo);

    const std::filesystem::path pipelineAbsolutePath{m_assetPaths.getPipelineConfigPath(storedPipelineInfo.filename)};
    CRISP_CHECK(exists(pipelineAbsolutePath), "Path {} doesn't exist!", pipelineAbsolutePath.string());

    auto pipelineResult = createPipelineFromFile(
        pipelineAbsolutePath,
        m_assetPaths.spvShaderDir,
        shaderCache,
        device,
        storedPipelineInfo.rasterizationPassDescriptor);

    auto& pipeline = m_pipelines.emplace(id, pipelineResult.unwrap()).first->second;

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

        auto pipelineResult = createPipelineFromFile(
            pipelineAbsolutePath, m_assetPaths.spvShaderDir, shaderCache, device, info.rasterizationPassDescriptor);

        auto pipeline = pipelineResult.unwrap();
        m_pipelines[id]->swapAll(*pipeline);
    }
}

} // namespace crisp
