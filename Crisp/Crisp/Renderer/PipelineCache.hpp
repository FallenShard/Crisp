#pragma once

#include <Crisp/Vulkan/DescriptorSetAllocator.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <string_view>

namespace crisp
{
class Renderer;

class PipelineCache
{
public:
    PipelineCache(Renderer* renderer);

    VulkanPipeline* loadPipeline(
        const std::string& id, std::string_view luaFilename, const VulkanRenderPass& renderPass, int subpassIndex);

    VulkanPipeline* getPipeline(const std::string& key) const;

    void recreatePipelines();

    inline DescriptorSetAllocator* getDescriptorAllocator(VulkanPipelineLayout* pipelineLayout)
    {
        return m_descriptorAllocators.at(pipelineLayout).get();
    }

private:
    Renderer* m_renderer;

    struct PipelineInfo
    {
        std::string luaFilename;
        const VulkanRenderPass* renderPass;
        int subpassIndex;
    };

    robin_hood::unordered_flat_map<std::string, PipelineInfo> m_pipelineInfos;
    robin_hood::unordered_flat_map<std::string, std::unique_ptr<VulkanPipeline>> m_pipelines;

    robin_hood::unordered_flat_map<VulkanPipelineLayout*, std::unique_ptr<DescriptorSetAllocator>>
        m_descriptorAllocators;
};
} // namespace crisp
