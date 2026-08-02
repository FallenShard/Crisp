#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <Crisp/Renderer/AssetPaths.hpp>
#include <Crisp/Renderer/ShaderCache.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDescriptorSetAllocator.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRasterizationPassDescriptor.hpp>

namespace crisp {

class PipelineCache {
public:
    explicit PipelineCache(AssetPaths assetPaths);

    VulkanPipeline* loadPipeline(
        const std::string& id,
        std::string_view filename,
        ShaderCache& shaderCache,
        VulkanDevice& device,
        const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor);

    VulkanPipeline* getPipeline(const std::string& key) const;

    void recreatePipelines(ShaderCache& shaderCache, const VulkanDevice& device);

    VulkanDescriptorSetAllocator* getDescriptorAllocator(VulkanPipelineLayout* pipelineLayout) {
        return m_descriptorAllocators.at(pipelineLayout).get();
    }

private:
    AssetPaths m_assetPaths;

    struct PipelineInfo {
        std::string filename;
        VulkanRasterizationPassDescriptor rasterizationPassDescriptor;
    };

    FlatHashMap<std::string, PipelineInfo> m_pipelineInfos;
    FlatHashMap<std::string, std::unique_ptr<VulkanPipeline>> m_pipelines;
    FlatHashMap<VulkanPipelineLayout*, std::unique_ptr<VulkanDescriptorSetAllocator>> m_descriptorAllocators;
};
} // namespace crisp
