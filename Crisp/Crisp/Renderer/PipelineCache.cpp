
#include <Crisp/Renderer/PipelineCache.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/ShaderUtils/ShaderCompiler.hpp>
#include <Crisp/Vulkan/VulkanPipelineIo.hpp>

namespace crisp {
PipelineCache::PipelineCache(AssetPaths assetPaths, const VkDescriptorSetLayout bindlessDescriptorSetLayout)
    : m_assetPaths(std::move(assetPaths))
    , m_bindlessDescriptorSetLayout(bindlessDescriptorSetLayout) {}

VulkanPipeline* PipelineCache::loadPipeline(
    const std::string& id, const std::string_view filename, VulkanDevice& device, const VulkanPipelineParams& params) {
    PipelineInfo pipelineInfo{.filename = std::string(filename), .params = params};

    auto& storedPipelineInfo = m_pipelineInfos[id];
    storedPipelineInfo = std::move(pipelineInfo);

    const std::filesystem::path pipelineAbsolutePath{m_assetPaths.getPipelineConfigPath(storedPipelineInfo.filename)};
    CRISP_CHECK(exists(pipelineAbsolutePath), "Path {} doesn't exist!", pipelineAbsolutePath.string());

    auto pipelineResult = createPipelineFromFile(
        pipelineAbsolutePath,
        m_assetPaths.spvShaderDir,
        device,
        m_bindlessDescriptorSetLayout,
        storedPipelineInfo.params);

    auto& pipeline = m_pipelines.emplace(id, pipelineResult.unwrap()).first->second;

    auto layout = pipeline->getPipelineLayout();
    if (layout) {
        m_descriptorAllocators[layout] = layout->createVulkanDescriptorSetAllocator(device);
    }

    return pipeline.get();
}

VulkanPipeline* PipelineCache::getPipeline(const std::string& key) const {
    return m_pipelines.at(key).get();
}

void PipelineCache::recreatePipelines(const VulkanDevice& device) {
    recompileShaderDir(m_assetPaths.shaderSourceDir, m_assetPaths.spvShaderDir).unwrap();

    for (auto& [id, info] : m_pipelineInfos) {
        const std::filesystem::path pipelineAbsolutePath{m_assetPaths.getPipelineConfigPath(info.filename)};
        CRISP_CHECK(exists(pipelineAbsolutePath), "Path {} doesn't exist!", pipelineAbsolutePath.string());

        auto pipelineResult = createPipelineFromFile(
            pipelineAbsolutePath, m_assetPaths.spvShaderDir, device, m_bindlessDescriptorSetLayout, info.params);

        auto pipeline = pipelineResult.unwrap();
        m_pipelines[id]->swapAll(*pipeline);
    }
}

} // namespace crisp
