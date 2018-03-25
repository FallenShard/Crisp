#pragma once

#include <vector>
#include <memory>

#include "CrispCore/BitFlags.hpp"

#include <vulkan/vulkan.h>
#include "vulkan/VulkanFormatTraits.hpp"

namespace crisp
{
    namespace internal
    {
        template <int location, int binding, VkFormat format, int offset>
        VkVertexInputAttributeDescription createVertexAttributeDescription()
        {
            return { location, binding, format, offset };
        }

        template <int loc, int binding, int offset>
        void fillVertexAttribs(std::vector<VkVertexInputAttributeDescription>& vertexAttribs) {
        }

        template <int loc, int binding, int offset, VkFormat format, VkFormat... formats>
        void fillVertexAttribs(std::vector<VkVertexInputAttributeDescription>& vertexAttribs) {
            vertexAttribs[loc] = createVertexAttributeDescription<loc, binding, format, offset>();

            fillVertexAttribs<loc + 1, binding, offset + FormatSizeof<format>::value, formats...>(vertexAttribs);
        }

        template <VkFormat... formats>
        std::vector<VkVertexInputAttributeDescription> generateVertexInputAttributes()
        {
            std::vector<VkVertexInputAttributeDescription> vertexAttribs(sizeof...(formats), VkVertexInputAttributeDescription{});
            fillVertexAttribs<0, 0, 0, formats...>(vertexAttribs);
            return vertexAttribs;
        }
    }

    enum class PipelineState
    {
        VertexInput   = 0x001,
        InputAssembly = 0x002,
        Tessellation  = 0x004,
        Viewport      = 0x008,
        Rasterization = 0x010,
        Multisample   = 0x020,
        ColorBlend    = 0x040,
        DepthStencil  = 0x080,
        Dynamic       = 0x100,

        Default = VertexInput | InputAssembly | Viewport | Rasterization | Multisample | ColorBlend | DepthStencil
    };
    DECLARE_BITFLAG(PipelineState);

    class PipelineBuilder
    {
    public:
        PipelineBuilder();

        PipelineBuilder& addShaderStage(VkPipelineShaderStageCreateInfo&& shaderStage);
        PipelineBuilder& setShaderStages(std::initializer_list<VkPipelineShaderStageCreateInfo> shaderStages);

        template <uint32_t binding, VkVertexInputRate inputRate, VkFormat... formats>
        PipelineBuilder& addVertexInputBinding() {
            m_vertexInputBindings.emplace_back(VkVertexInputBindingDescription{ binding, FormatSizeof<formats...>::value, inputRate });
            m_vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindings.size());
            m_vertexInputState.pVertexBindingDescriptions    = m_vertexInputBindings.data();
            return *this;
        }

        template <VkFormat... formats>
        PipelineBuilder& setVertexAttributes() {
            m_vertexInputAttributes = internal::generateVertexInputAttributes<formats...>();
            m_vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributes.size());
            m_vertexInputState.pVertexAttributeDescriptions    = m_vertexInputAttributes.data();
            return *this;
        }

        PipelineBuilder& setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);

        PipelineBuilder& setViewport(VkViewport&& viewport);
        PipelineBuilder& setScissor(VkRect2D&& scissor);

        void enableState(PipelineState pipelineState);
        void disableState(PipelineState pipelineState);

        VkPipeline create(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, uint32_t subpassIndex);

    private:
        VkPipelineRasterizationStateCreateInfo createDefaultRasterizationState();
        VkPipelineMultisampleStateCreateInfo   createDefaultMultisampleState();
        VkPipelineColorBlendAttachmentState    createDefaultColorBlendAttachmentState();
        VkPipelineColorBlendStateCreateInfo    createDefaultColorBlendState();
        VkPipelineDepthStencilStateCreateInfo  createDefaultDepthStencilState();

        std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

        std::vector<VkVertexInputBindingDescription>   m_vertexInputBindings;
        std::vector<VkVertexInputAttributeDescription> m_vertexInputAttributes;
        VkPipelineVertexInputStateCreateInfo           m_vertexInputState;

        VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyState;

        VkPipelineTessellationStateCreateInfo m_tessellationState;

        std::vector<VkViewport>           m_viewports;
        std::vector<VkRect2D>             m_scissors;
        VkPipelineViewportStateCreateInfo m_viewportState;

        VkPipelineRasterizationStateCreateInfo m_rasterizationState;

        VkPipelineMultisampleStateCreateInfo m_multisampleState;

        std::vector<VkPipelineColorBlendAttachmentState> m_colorBlendAttachmentStates;
        VkPipelineColorBlendStateCreateInfo              m_colorBlendState;

        VkPipelineDepthStencilStateCreateInfo m_depthStencilState;

        VkPipelineDynamicStateCreateInfo m_dynamicState;

        PipelineStateFlags m_pipelineStateFlags;
    };
}