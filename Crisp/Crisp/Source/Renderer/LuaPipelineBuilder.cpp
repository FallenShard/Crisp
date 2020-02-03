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
        readViewportState(renderer, renderPass);

        PipelineLayoutBuilder layoutBuilder(reflection);
        readDescriptorSetBufferedStatus(layoutBuilder);
        readDynamicBufferDescriptorIds(layoutBuilder);
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
        readVertexInputBindings();
        readVertexAttributes();
    }

    void LuaPipelineBuilder::readVertexInputBindings()
    {
        std::size_t numInputBindings = m_config["vertexInputBindings"].getLength();
        for (uint32_t i = 0; i < numInputBindings; ++i)
        {
            std::vector<VkFormat> bindingFormats;
            VkVertexInputRate inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;

            auto inputBinding = m_config["vertexInputBindings"][i].convertTo<std::vector<std::string>>().value();
            for (const auto& f : inputBinding)
            {
                if (f == "vec3")
                    bindingFormats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
                else if (f == "vec2")
                    bindingFormats.push_back(VK_FORMAT_R32G32_SFLOAT);
                else if (f == "instance")
                    inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
                else if (f == "vertex")
                    inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                else
                    logError("Unknown format: {}\n", f);
            }
            m_builder.addVertexInputBinding(i, inputRate, bindingFormats);
        }
    }

    void LuaPipelineBuilder::readVertexAttributes()
    {
        std::size_t numAttributes = m_config["vertexAttribs"].getLength();
        for (uint32_t i = 0; i < numAttributes; ++i)
        {
            std::vector<VkFormat> vulkanFormats;
            auto strFormats = m_config["vertexAttribs"][i].convertTo<std::vector<std::string>>().value();
            for (const auto& format : strFormats)
            {
                if (format == "vec3")
                    vulkanFormats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
                else if (format == "vec2")
                    vulkanFormats.push_back(VK_FORMAT_R32G32_SFLOAT);
                else
                    logError("Unknown format: {}\n", format);
            }
            m_builder.addVertexAttributes(i, vulkanFormats);
        }
    }

    void LuaPipelineBuilder::readViewportState(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        if (auto viewport = m_config.get<std::vector<float>>("viewport"); viewport && viewport.value().size() == 4)
        {
            const auto& vec = viewport.value();
            m_builder.setViewport(VkViewport{ vec[0], vec[1], vec[2], vec[3] });
        }
        else if (auto viewport = m_config.get<std::string>("viewport"); viewport == "pass")
        {
            m_builder.setViewport(renderPass->createViewport());
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
        else if (auto scissor = m_config.get<std::string>("scissor"); scissor == "pass")
        {
            m_builder.setScissor(renderPass->createScissor());
        }
        else
        {
            m_builder.setScissor(renderer->getDefaultScissor())
                .addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
        }
    }

    void LuaPipelineBuilder::readDescriptorSetBufferedStatus(PipelineLayoutBuilder& layoutBuilder)
    {
        if (m_config.hasVariable("setBuffering"))
            if (auto setBuffering = m_config.get<std::vector<bool>>("setBuffering"))
                for (uint32_t i = 0; i < setBuffering.value().size(); ++i)
                    layoutBuilder.setDescriptorSetBuffering(i, setBuffering.value()[i]);
    }

    void LuaPipelineBuilder::readDynamicBufferDescriptorIds(PipelineLayoutBuilder& layoutBuilder)
    {
        if (m_config.hasVariable("dynamicBuffers"))
        {
            std::size_t numDescriptorSets = m_config["dynamicBuffers"].getLength();
            for (uint32_t i = 0; i < numDescriptorSets; ++i)
            {
                auto bindings = m_config["dynamicBuffers"][i].convertTo<std::vector<uint32_t>>().value();
                for (uint32_t binding : bindings)
                    layoutBuilder.setDescriptorDynamic(i, binding, true);
            }
        }
    }
}
