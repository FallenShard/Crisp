#pragma once

#include <vector>
#include <memory>

#include "CrispCore/BitFlags.hpp"

#include <vulkan/vulkan.h>
#include "vulkan/VulkanFormatTraits.hpp"
#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class VulkanDevice;

    namespace internal
    {
        template <uint32_t loc, uint32_t binding, int offset, VkFormat format, VkFormat... formats>
        void fillVertexAttribs(std::vector<VkVertexInputAttributeDescription>& vertexAttribs) {
            vertexAttribs.emplace_back(VkVertexInputAttributeDescription{ loc, binding, format, offset });

            if constexpr (sizeof...(formats) > 0)
                fillVertexAttribs<loc + 1, binding, offset + FormatSizeof<format>::value, formats...>(vertexAttribs);
        }

        template <uint32_t loc, uint32_t binding, VkFormat... formats>
        std::vector<VkVertexInputAttributeDescription> generateVertexInputAttributes()
        {
            std::vector<VkVertexInputAttributeDescription> vertexAttribs;
            fillVertexAttribs<loc, binding, 0, formats...>(vertexAttribs);
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
        PipelineBuilder& setShaderStages(std::vector<VkPipelineShaderStageCreateInfo>&& shaderStages);

        template <uint32_t binding, VkVertexInputRate inputRate, VkFormat... formats>
        PipelineBuilder& addVertexInputBinding() {
            m_vertexInputBindings.emplace_back(VkVertexInputBindingDescription{ binding, FormatSizeof<formats...>::value, inputRate });
            m_vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindings.size());
            m_vertexInputState.pVertexBindingDescriptions    = m_vertexInputBindings.data();
            return *this;
        }

        template <VkFormat... formats>
        PipelineBuilder& setVertexAttributes() {
            m_vertexInputAttributes = internal::generateVertexInputAttributes<0, 0, formats...>();
            m_vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributes.size());
            m_vertexInputState.pVertexAttributeDescriptions    = m_vertexInputAttributes.data();
            return *this;
        }

        template <uint32_t startLoc, uint32_t binding, VkFormat... formats>
        PipelineBuilder& addVertexAttributes() {
            auto attribs = internal::generateVertexInputAttributes<startLoc, binding, formats...>();
            m_vertexInputAttributes.insert(m_vertexInputAttributes.end(), attribs.begin(), attribs.end());
            m_vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributes.size());
            m_vertexInputState.pVertexAttributeDescriptions = m_vertexInputAttributes.data();
            return *this;
        }

        PipelineBuilder& addVertexInputBinding(uint32_t binding, VkVertexInputRate inputRate, const std::vector<VkFormat>& formats);
        PipelineBuilder& addVertexAttributes(uint32_t binding, const std::vector<VkFormat>& formats);

        PipelineBuilder& setFullScreenVertexLayout();

        PipelineBuilder& setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);
        PipelineBuilder& setTessellationControlPoints(uint32_t numControlPoints);

        PipelineBuilder& setPolygonMode(VkPolygonMode polygonMode);
        PipelineBuilder& setFrontFace(VkFrontFace frontFace);
        PipelineBuilder& setCullMode(VkCullModeFlags cullMode);
        PipelineBuilder& setLineWidth(float lineWidth);

        PipelineBuilder& setViewport(VkViewport&& viewport);
        PipelineBuilder& setScissor(VkRect2D&& scissor);

        PipelineBuilder& setBlendState(uint32_t index, VkBool32 enabled);
        PipelineBuilder& setBlendFactors(uint32_t index, VkBlendFactor srcFactor, VkBlendFactor dstFactor);

        PipelineBuilder& setDepthTest(VkBool32 enabled);
        PipelineBuilder& setDepthWrite(VkBool32 enabled);

        PipelineBuilder& enableState(PipelineState pipelineState);
        PipelineBuilder& disableState(PipelineState pipelineState);

        PipelineBuilder& addDynamicState(VkDynamicState dynamicState);

        std::unique_ptr<VulkanPipeline> create(VulkanDevice* device, std::unique_ptr<VulkanPipelineLayout> pipelineLayout, VkRenderPass renderPass, uint32_t subpassIndex);
        PipelineDynamicStateFlags createDynamicStateFlags() const;

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

        std::vector<VkDynamicState>      m_dynamicStates;
        VkPipelineDynamicStateCreateInfo m_dynamicState;

        PipelineStateFlags m_pipelineStateFlags;
    };

    VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint = "main");
}