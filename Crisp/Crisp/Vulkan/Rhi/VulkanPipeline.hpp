#pragma once

#include <filesystem>
#include <utility>

#include <Crisp/Utils/BitFlags.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDescriptorSet.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipelineLayout.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>
#include <Crisp/Vulkan/VulkanVertexLayout.hpp>

namespace crisp {
enum class PipelineDynamicState { None = 0x00, Viewport = 0x01, Scissor = 0x02 };
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

    inline VulkanPipelineLayout* getPipelineLayout() const {
        return m_pipelineLayout.get();
    }

    void bind(VkCommandBuffer buffer) const;

    VulkanDescriptorSet allocateDescriptorSet(uint32_t setId) const;

    inline VkDescriptorType getDescriptorType(uint32_t setIndex, uint32_t binding) const {
        return m_pipelineLayout->getDescriptorType(setIndex, binding);
    }

    template <typename T>
    inline void setPushConstant(
        VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, uint32_t offset, T&& value) const {
        vkCmdPushConstants(cmdBuffer, m_pipelineLayout->getHandle(), shaderStages, offset, sizeof(T), &value);
    }

    inline void setPushConstant(
        VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, uint32_t offset, uint32_t size, const char* value)
        const {
        vkCmdPushConstants(
            cmdBuffer, m_pipelineLayout->getHandle(), shaderStages, offset, size, value + offset); // NOLINT
    }

    template <typename T, typename... Ts>
    inline void setPushConstants(
        VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, T&& arg, Ts&&... args) const {
        setPushConstantsWithOffset(cmdBuffer, shaderStages, 0, std::forward<T>(arg), std::forward<Ts>(args)...);
    }

    inline PipelineDynamicStateFlags getDynamicStateFlags() const {
        return m_dynamicStateFlags;
    }

    inline const VulkanVertexLayout& getVertexLayout() const {
        return m_vertexLayout;
    }

    inline void setConfigPath(std::filesystem::path path) {
        m_configPath = std::move(path);
    }

    VkPipelineBindPoint getBindPoint() const {
        return m_bindPoint;
    }

    void swapAll(VulkanPipeline& other);

protected:
    template <typename T, typename... Ts>
    inline void setPushConstantsWithOffset(
        VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, uint32_t offset, T&& arg, Ts&&... args) const {
        setPushConstant(cmdBuffer, shaderStages, offset, std::forward<T>(arg));

        if constexpr (sizeof...(Ts) > 0) {
            setPushConstantsWithOffset(cmdBuffer, shaderStages, offset + sizeof(T), std::forward<Ts>(args)...);
        }
    }

    std::unique_ptr<VulkanPipelineLayout> m_pipelineLayout;
    PipelineDynamicStateFlags m_dynamicStateFlags;
    VulkanVertexLayout m_vertexLayout;

    std::filesystem::path m_configPath; // Config file where the pipeline is described.
    const VkPipelineBindPoint m_bindPoint;
};
} // namespace crisp