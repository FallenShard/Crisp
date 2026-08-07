#pragma once

#include <filesystem>
#include <string_view>
#include <utility>

#include <Crisp/Utils/BitFlags.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDescriptorSet.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipelineLayout.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>
#include <Crisp/Vulkan/VulkanVertexLayout.hpp>

namespace crisp {
enum class PipelineDynamicState : uint8_t { None = 0x00, Viewport = 0x01, Scissor = 0x02 };
DECLARE_BITFLAG(PipelineDynamicState);

class VulkanPipeline final : public VulkanResource<VkPipeline> {
public:
    VulkanPipeline(
        const VulkanDevice& device,
        VkPipeline pipelineHandle,
        std::unique_ptr<VulkanPipelineLayout> pipelineLayout,
        VkPipelineBindPoint bindPoint,
        VulkanVertexLayout&& vertexLayout = {},
        PipelineDynamicStateFlags dynamicStateFlags = PipelineDynamicState::None);

    VulkanPipelineLayout* getPipelineLayout() const {
        return m_pipelineLayout.get();
    }

    void setDebugName(const VulkanDevice& device, std::string_view name) const;

    VulkanDescriptorSet allocateDescriptorSet(uint32_t setId) const;

    VkDescriptorType getDescriptorType(uint32_t setIndex, uint32_t binding) const {
        return m_pipelineLayout->getDescriptorType(setIndex, binding);
    }

    PipelineDynamicStateFlags getDynamicStateFlags() const {
        return m_dynamicStateFlags;
    }

    const VulkanVertexLayout& getVertexLayout() const {
        return m_vertexLayout;
    }

    void setConfigPath(std::filesystem::path path) {
        m_configPath = std::move(path);
    }

    VkPipelineBindPoint getBindPoint() const {
        return m_bindPoint;
    }

    void swapAll(VulkanPipeline& other);

protected:
    std::unique_ptr<VulkanPipelineLayout> m_pipelineLayout;
    PipelineDynamicStateFlags m_dynamicStateFlags;
    VulkanVertexLayout m_vertexLayout;

    std::filesystem::path m_configPath; // Config file where the pipeline is described.
    const VkPipelineBindPoint m_bindPoint;
};
} // namespace crisp
