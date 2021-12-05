#pragma once

#include <CrispCore/LuaConfig.hpp>

#include <Crisp/Renderer/PipelineBuilder.hpp>

namespace crisp
{
    class VulkanRenderPass;
    class Renderer;
    class PipelineLayoutBuilder;

    class LuaPipelineBuilder
    {
    public:
        LuaPipelineBuilder();
        LuaPipelineBuilder(std::filesystem::path configPath);

        std::unique_ptr<VulkanPipeline> create(Renderer* renderer, const VulkanRenderPass& renderPass, uint32_t subpassIndex = 0);

    private:
        std::unordered_map<VkShaderStageFlagBits, std::string> getShaderFileMap();
        void readVertexInputState();
        void readVertexInputBindings();
        void readVertexAttributes();
        void readInputAssemblyState();
        void readTessellationState();
        void readViewportState(Renderer* renderer, const VulkanRenderPass& renderPass);
        void readRasterizationState();
        void readMultisampleState(const VulkanRenderPass& renderPass);
        void readBlendState();
        void readDepthStencilState();

        std::vector<bool> readDescriptorSetBufferedStatus();
        std::vector<std::pair<uint32_t, uint32_t>> readDynamicBufferDescriptorIds();

        PipelineBuilder m_builder;
        LuaConfig       m_config;
        std::string m_configName;
    };
}