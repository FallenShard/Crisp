#pragma once

#include <Crisp/Renderer/AssetPaths.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDescriptorSetAllocator.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

#include <string_view>

namespace crisp {
class Renderer;

class PipelineCache {
public:
    PipelineCache(const AssetPaths& assetPaths);

    VulkanPipeline* loadPipeline(
        Renderer* renderer,
        const std::string& id,
        std::string_view luaFilename,
        const VulkanRenderPass& renderPass,
        int subpassIndex);

    VulkanPipeline* getPipeline(const std::string& key) const;

    void recreatePipelines(Renderer& renderer);

    inline VulkanDescriptorSetAllocator* getDescriptorAllocator(VulkanPipelineLayout* pipelineLayout) {
        return m_descriptorAllocators.at(pipelineLayout).get();
    }

private:
    AssetPaths m_assetPaths;

    struct PipelineInfo {
        std::string luaFilename;
        const VulkanRenderPass* renderPass;
        int subpassIndex;
    };

    FlatHashMap<std::string, PipelineInfo> m_pipelineInfos;
    FlatHashMap<std::string, std::unique_ptr<VulkanPipeline>> m_pipelines;
    FlatHashMap<VulkanPipelineLayout*, std::unique_ptr<VulkanDescriptorSetAllocator>> m_descriptorAllocators;
};
} // namespace crisp
