#pragma once

#include "PipelineBuilder.hpp"
#include "Core/LuaConfig.hpp"

#include <unordered_map>

namespace crisp
{
    class VulkanRenderPass;
    class Renderer;

    class LuaPipelineBuilder
    {
    public:
        LuaPipelineBuilder();
        LuaPipelineBuilder(std::filesystem::path configPath);

        std::unique_ptr<VulkanPipeline> create(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpassIndex = 0);

    private:
        std::unordered_map<VkShaderStageFlagBits, std::string> getShaderFileMap();
        void readVertexInputState();
        void readViewportState(Renderer* renderer);

        PipelineBuilder m_builder;
        LuaConfig       m_config;
    };
}