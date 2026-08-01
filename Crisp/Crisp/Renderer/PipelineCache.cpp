
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
    return loadPipelineImpl(
        id,
        shaderCache,
        device,
        {.filename = std::string(filename), .renderPass = &renderPass, .subpassIndex = subpassIndex});
}

VulkanPipeline* PipelineCache::loadPipeline(
    const std::string& id,
    const std::string_view filename,
    ShaderCache& shaderCache,
    VulkanDevice& device,
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor) {
    return loadPipelineImpl(
        id,
        shaderCache,
        device,
        {
            .filename = std::string(filename),
            .rasterizationPassDescriptor = rasterizationPassDescriptor,
        });
}

VulkanPipeline* PipelineCache::loadPipelineImpl(
    const std::string& id, ShaderCache& shaderCache, VulkanDevice& device, PipelineInfo pipelineInfo) {
    auto& storedPipelineInfo = m_pipelineInfos[id];
    storedPipelineInfo = std::move(pipelineInfo);

    const std::filesystem::path pipelineAbsolutePath{m_assetPaths.getPipelineConfigPath(storedPipelineInfo.filename)};
    CRISP_CHECK(exists(pipelineAbsolutePath), "Path {} doesn't exist!", pipelineAbsolutePath.string());

    auto pipelineResult = [&]() -> Result<std::unique_ptr<VulkanPipeline>> {
        if (storedPipelineInfo.rasterizationPassDescriptor) {
            return createPipelineFromFile(
                pipelineAbsolutePath,
                m_assetPaths.spvShaderDir,
                shaderCache,
                device,
                *storedPipelineInfo.rasterizationPassDescriptor);
        }

        CRISP_CHECK(storedPipelineInfo.renderPass != nullptr);
        return createPipelineFromFile(
            pipelineAbsolutePath,
            m_assetPaths.spvShaderDir,
            shaderCache,
            device,
            *storedPipelineInfo.renderPass,
            storedPipelineInfo.subpassIndex);
    }();

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

        auto pipelineResult = [&]() -> Result<std::unique_ptr<VulkanPipeline>> {
            if (info.rasterizationPassDescriptor) {
                return createPipelineFromFile(
                    pipelineAbsolutePath,
                    m_assetPaths.spvShaderDir,
                    shaderCache,
                    device,
                    *info.rasterizationPassDescriptor);
            }

            CRISP_CHECK(info.renderPass != nullptr);
            return createPipelineFromFile(
                pipelineAbsolutePath, m_assetPaths.spvShaderDir, shaderCache, device, *info.renderPass, info.subpassIndex);
        }();

        auto pipeline = pipelineResult.unwrap();
        m_pipelines[id]->swapAll(*pipeline);
    }
}

} // namespace crisp
