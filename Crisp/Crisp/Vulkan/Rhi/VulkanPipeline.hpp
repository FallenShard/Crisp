#pragma once

#include <filesystem>
#include <flat_map>
#include <string_view>
#include <utility>
#include <variant>

#include <Crisp/Utils/BitFlags.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDescriptorSet.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipelineLayout.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>
#include <Crisp/Vulkan/VulkanVertexLayout.hpp>

namespace crisp {
using SpecializationConstant = std::variant<uint32_t, int32_t, float>;
using SpecializationConstantMap = std::flat_map<uint32_t, SpecializationConstant>;

// Only states that Vulkan 1.3 promoted to core are listed; the engine requires 1.4, so none of these need a feature
// query. The VK_EXT_extended_dynamic_state3 states (polygon mode, per-attachment blending, ...) stayed optional and
// would need one, so they are deliberately absent.
enum class PipelineDynamicState : uint32_t {
    None = 0x0000'0000,

    // Vulkan 1.0.
    Viewport = 0x0000'0001,
    Scissor = 0x0000'0002,
    LineWidth = 0x0000'0004,
    DepthBias = 0x0000'0008,
    BlendConstants = 0x0000'0010,
    DepthBounds = 0x0000'0020,
    StencilCompareMask = 0x0000'0040,
    StencilWriteMask = 0x0000'0080,
    StencilReference = 0x0000'0100,

    // VK_EXT_extended_dynamic_state, core in 1.3.
    CullMode = 0x0000'0200,
    FrontFace = 0x0000'0400,
    PrimitiveTopology = 0x0000'0800,
    DepthTestEnable = 0x0000'1000,
    DepthWriteEnable = 0x0000'2000,
    DepthCompareOp = 0x0000'4000,
    DepthBoundsTestEnable = 0x0000'8000,
    StencilTestEnable = 0x0001'0000,
    StencilOp = 0x0002'0000,

    // VK_EXT_extended_dynamic_state2, core in 1.3.
    RasterizerDiscardEnable = 0x0004'0000,
    DepthBiasEnable = 0x0008'0000,
    PrimitiveRestartEnable = 0x0010'0000,
};
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
