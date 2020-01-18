#include "LuaPipelineBuilder.hpp"

#include <CrispCore/Log.hpp>

#include "ShadingLanguage/Reflection.hpp"
#include "Renderer.hpp"
#include "Vulkan/VulkanRenderPass.hpp"

#include "PipelineLayoutBuilder.hpp"

namespace crisp
{
    LuaPipelineBuilder::LuaPipelineBuilder()
    {
    }

    LuaPipelineBuilder::LuaPipelineBuilder(std::filesystem::path configPath)
        : m_config(configPath)
    {
    }

    std::unique_ptr<VulkanPipeline> LuaPipelineBuilder::create(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpassIndex)
    {
        const auto shaderFileMap = getShaderFileMap();
        sl::Reflection reflection;
        for (const auto& [stageFlag, fileStem] : shaderFileMap)
        {
            reflection.parseDescriptorSets(renderer->getShaderSourcePath(fileStem));
            m_builder.addShaderStage(createShaderStageInfo(stageFlag, renderer->getShaderModule(fileStem)));
        }

        readVertexInputState();
        readViewportState(renderer);

        PipelineLayoutBuilder layoutBuilder(reflection);
        return m_builder.create(renderer->getDevice(), layoutBuilder.create(renderer->getDevice()), renderPass->getHandle(), subpassIndex);
    }

    std::unordered_map<VkShaderStageFlagBits, std::string> LuaPipelineBuilder::getShaderFileMap()
    {
        std::unordered_map<VkShaderStageFlagBits, std::string> shaderFiles;

        if (auto shaderFilename = m_config["shaders"].get<std::string>("vert"))
            shaderFiles.emplace(VK_SHADER_STAGE_VERTEX_BIT, shaderFilename.value());

        if (auto shaderFilename = m_config["shaders"].get<std::string>("frag"))
            shaderFiles.emplace(VK_SHADER_STAGE_FRAGMENT_BIT, shaderFilename.value());

        if (auto shaderFilename = m_config["shaders"].get<std::string>("geom"))
            shaderFiles.emplace(VK_SHADER_STAGE_GEOMETRY_BIT, shaderFilename.value());

        if (auto shaderFilename = m_config["shaders"].get<std::string>("tesc"))
            shaderFiles.emplace(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shaderFilename.value());

        if (auto shaderFilename = m_config["shaders"].get<std::string>("tese"))
            shaderFiles.emplace(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shaderFilename.value());

        if (auto shaderFilename = m_config["shaders"].get<std::string>("comp"))
            shaderFiles.emplace(VK_SHADER_STAGE_COMPUTE_BIT, shaderFilename.value());

        return shaderFiles;
    }

    void LuaPipelineBuilder::readVertexInputState()
    {
        auto inputBinding = m_config.get<std::vector<std::string>>("vertexInputBindings").value();
        std::vector<VkFormat> bindingFormats;
        VkVertexInputRate inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
        for (const auto& f : inputBinding)
        {
            if (f == "vec3")
                bindingFormats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
            else if (f == "vec2")
                bindingFormats.push_back(VK_FORMAT_R32G32_SFLOAT);
            else if (f == "instance")
                inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        }
        m_builder.addVertexInputBinding(0, inputRate, bindingFormats);

        auto format = m_config.get<std::vector<std::string>>("vertexAttribs").value_or(std::vector<std::string>());
        std::vector<VkFormat> formats;
        for (const auto& f : format)
        {
            if (f == "vec3")
                formats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
            else if (f == "vec2")
                formats.push_back(VK_FORMAT_R32G32_SFLOAT);
        }
        m_builder.addVertexAttributes(0, formats);
    }

    void LuaPipelineBuilder::readViewportState(Renderer* renderer)
    {
        if (auto viewport = m_config.get<std::vector<float>>("viewport"); viewport && viewport.value().size() == 4)
        {
            const auto& vec = viewport.value();
            m_builder.setViewport(VkViewport{ vec[0], vec[1], vec[2], vec[3] });
        }
        else
        {
            m_builder.setViewport(renderer->getDefaultViewport())
                .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
        }

        if (auto scissor = m_config.get<std::vector<int32_t>>("scissor"); scissor && scissor.value().size() == 4)
        {
            const auto& vec = scissor.value();
            m_builder.setScissor(VkRect2D{ { vec[0], vec[1] }, { static_cast<uint32_t>(vec[2]), static_cast<uint32_t>(vec[3]) } });
        }
        else
        {
            m_builder.setScissor(renderer->getDefaultScissor())
                .addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
        }
    }
}
