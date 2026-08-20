#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <Crisp/Renderer/AssetPaths.hpp>
#include <Crisp/Vulkan/PipelineBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDescriptorSetAllocator.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRasterizationPassDescriptor.hpp>

namespace crisp {

class PipelineCache {
public:
    PipelineCache(AssetPaths assetPaths, VkDescriptorSetLayout bindlessDescriptorSetLayout);

    VulkanPipeline* loadPipeline(
        const std::string& id,
        std::string_view filename,
        VulkanDevice& device,
        const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor,
        const SpecializationConstantMap& specializationConstants = {});

    VulkanPipeline* getPipeline(const std::string& key) const;

    void recreatePipelines(const VulkanDevice& device);

    VulkanDescriptorSetAllocator* getDescriptorAllocator(VulkanPipelineLayout* pipelineLayout) {
        return m_descriptorAllocators.at(pipelineLayout).get();
    }

private:
    AssetPaths m_assetPaths;
    VkDescriptorSetLayout m_bindlessDescriptorSetLayout;

    struct PipelineInfo {
        std::string filename;
        VulkanRasterizationPassDescriptor rasterizationPassDescriptor;
        SpecializationConstantMap specializationConstants;
    };

    FlatHashMap<std::string, PipelineInfo> m_pipelineInfos;
    FlatHashMap<std::string, std::unique_ptr<VulkanPipeline>> m_pipelines;
    FlatHashMap<VulkanPipelineLayout*, std::unique_ptr<VulkanDescriptorSetAllocator>> m_descriptorAllocators;
};
} // namespace crisp
