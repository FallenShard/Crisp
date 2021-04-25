#pragma once

#include "Vulkan/VulkanResource.hpp"
#include "Vulkan/VulkanFormatTraits.hpp"
#include "Vulkan/VulkanDescriptorSetLayout.hpp"
#include "Vulkan/VulkanDescriptorSet.hpp"
#include "Vulkan/VulkanPipelineLayout.hpp"

#include <CrispCore/BitFlags.hpp>
#include <CrispCore/Math/Headers.hpp>

#include <filesystem>

namespace crisp
{
    class Renderer;
    class VulkanRenderPass;
    class VulkanDevice;

    enum class PipelineDynamicState
    {
        Unknown  = 0x00,
        Viewport = 0x01,
        Scissor  = 0x02
    };
    DECLARE_BITFLAG(PipelineDynamicState);

    class VulkanPipeline final : public VulkanResource<VkPipeline>
    {
    public:
        VulkanPipeline(VulkanDevice* device, VkPipeline pipelineHandle, std::unique_ptr<VulkanPipelineLayout> pipelineLayout, PipelineDynamicStateFlags dynamicStateFlags);
        virtual ~VulkanPipeline();

        inline VulkanPipelineLayout* getPipelineLayout() const { return m_pipelineLayout.get(); }

        void setBindPoint(VkPipelineBindPoint pipelineBindPoint) { m_bindPoint = pipelineBindPoint; }

        void bind(VkCommandBuffer buffer) const;

        VulkanDescriptorSet allocateDescriptorSet(uint32_t setId) const;

        inline VkDescriptorType getDescriptorType(uint32_t setIndex, uint32_t binding) const
        {
            return m_pipelineLayout->getDescriptorType(setIndex, binding);
        }

        template <typename T>
        inline void setPushConstant(VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, uint32_t offset, T&& value) const
        {
            vkCmdPushConstants(cmdBuffer, m_pipelineLayout->getHandle(), shaderStages, offset, sizeof(T), &value);
        }

        inline void setPushConstant(VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, uint32_t offset, uint32_t size, const char* value) const
        {
            vkCmdPushConstants(cmdBuffer, m_pipelineLayout->getHandle(), shaderStages, offset, size, value + offset);
        }

        template <typename T, typename ...Ts>
        inline void setPushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, T&& arg, Ts&&... args) const
        {
            setPushConstantsWithOffset(cmdBuffer, shaderStages, 0, std::forward<T>(arg), std::forward<Ts>(args)...);
        }

        inline PipelineDynamicStateFlags getDynamicStateFlags() const { return m_dynamicStateFlags; }

        inline void setLuaFilepath(std::filesystem::path path)
        {
            m_luaFilepath = path;
        }

    protected:
        template <typename T, typename ...Ts>
        inline void setPushConstantsWithOffset(VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, uint32_t offset, T&& arg, Ts&&... args) const
        {
            setPushConstant(cmdBuffer, shaderStages, offset, std::forward<T>(arg));

            if constexpr (sizeof...(Ts) > 0)
                setPushConstantsWithOffset(cmdBuffer, shaderStages, offset + sizeof(T), std::forward<Ts>(args)...);
        }

        std::unique_ptr<VulkanPipelineLayout> m_pipelineLayout;
        PipelineDynamicStateFlags m_dynamicStateFlags;

        std::filesystem::path m_luaFilepath; // Config file where the pipeline is described
        const VulkanRenderPass* m_renderPass;
        const uint32_t m_subpassIndex;

        VkPipelineBindPoint m_bindPoint;

    };
}