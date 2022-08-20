#include <Crisp/Renderer/PipelineCache.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/ShadingLanguage/ShaderCompiler.hpp>

namespace crisp
{
PipelineCache::PipelineCache(Renderer* renderer)
    : m_renderer(renderer)
{
}

VulkanPipeline* PipelineCache::loadPipeline(
    const std::string& id,
    const std::string_view luaFilename,
    const VulkanRenderPass& renderPass,
    const int subpassIndex)
{
    auto& pipelineInfo = m_pipelineInfos[id];
    pipelineInfo.luaFilename = luaFilename;
    pipelineInfo.renderPass = &renderPass;
    pipelineInfo.subpassIndex = subpassIndex;

    auto& pipeline =
        m_pipelines.emplace(id, m_renderer->createPipelineFromLua(luaFilename, renderPass, subpassIndex)).first->second;
    pipeline->setTag(id);

    auto layout = pipeline->getPipelineLayout();
    m_descriptorAllocators[layout] = layout->createDescriptorSetAllocator(m_renderer->getDevice());

    return pipeline.get();
}

VulkanPipeline* PipelineCache::getPipeline(const std::string& key) const
{
    return m_pipelines.at(key).get();
}

void PipelineCache::recreatePipelines()
{
    recompileShaderDir(
        ApplicationEnvironment::getShaderSourceDirectory(), ApplicationEnvironment::getResourcesPath() / "Shaders");

    m_renderer->enqueueResourceUpdate(
        [this](VkCommandBuffer)
        {
            for (auto& [id, info] : m_pipelineInfos)
            {
                auto pipeline =
                    m_renderer->createPipelineFromLua(info.luaFilename, *info.renderPass, info.subpassIndex);
                m_pipelines[id]->swapAll(*pipeline);
            }
        });
}

} // namespace crisp
