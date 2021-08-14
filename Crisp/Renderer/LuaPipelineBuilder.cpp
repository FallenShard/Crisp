#include "LuaPipelineBuilder.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "ShadingLanguage/Reflection.hpp"
#include "Renderer.hpp"
#include "Vulkan/VulkanRenderPass.hpp"

#include "PipelineLayoutBuilder.hpp"

namespace
{
    auto logger = spdlog::stdout_color_mt("LuaPipelineBuilder");
}

namespace crisp
{
    LuaPipelineBuilder::LuaPipelineBuilder()
    {
    }

    LuaPipelineBuilder::LuaPipelineBuilder(std::filesystem::path configPath)
        : m_config(configPath)
    {
    }

    std::unique_ptr<VulkanPipeline> LuaPipelineBuilder::create(Renderer* renderer, const VulkanRenderPass& renderPass, uint32_t subpassIndex)
    {
        const auto shaderFileMap = getShaderFileMap();
        sl::Reflection reflection;
        for (const auto& [stageFlag, fileStem] : shaderFileMap)
        {
            renderer->loadShaderModule(fileStem);
            reflection.parseDescriptorSets(renderer->getShaderSourcePath(fileStem));
            m_builder.addShaderStage(createShaderStageInfo(stageFlag, renderer->getShaderModule(fileStem)));
        }

        if (shaderFileMap.size() == 4)
            m_builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);

        readVertexInputState();
        readInputAssemblyState();
        readTessellationState();
        readViewportState(renderer, renderPass);
        readRasterizationState();
        readMultisampleState(renderPass);
        readBlendState();
        readDepthStencilState();

        PipelineLayoutBuilder layoutBuilder(reflection);
        readDescriptorSetBufferedStatus(layoutBuilder);
        readDynamicBufferDescriptorIds(layoutBuilder);
        return m_builder.create(renderer->getDevice(), layoutBuilder.create(renderer->getDevice()), renderPass.getHandle(), subpassIndex);
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
                else if (f == "vec4")
                    bindingFormats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
                else if (f == "instance")
                    inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
                else if (f == "vertex")
                    inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                else
                    logger->error("Unknown format: {}", f);
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
                else if (format == "vec4")
                    vulkanFormats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
                else
                    logger->error("Unknown format: {}", format);
            }
            m_builder.addVertexAttributes(i, vulkanFormats);
        }
    }

    void LuaPipelineBuilder::readInputAssemblyState()
    {
        if (!m_config.hasVariable("inputAssembly"))
            return;

        const auto& inputAssembly = m_config["inputAssembly"];
        if (auto topology = inputAssembly.get<std::string>("primitiveTopology"))
        {
            if (topology == "lineList")
                m_builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
            else if (topology == "pointList")
                m_builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
        }
    }

    void LuaPipelineBuilder::readTessellationState()
    {
        if (!m_config.hasVariable("tessellation"))
            return;

        const auto& tessellation = m_config["tessellation"];
        if (auto controlPointCount = tessellation.get<int>("controlPointCount"))
        {
            if (controlPointCount.has_value())
                m_builder.setTessellationControlPoints(controlPointCount.value());
        }
    }

    void LuaPipelineBuilder::readViewportState(Renderer* renderer, const VulkanRenderPass& renderPass)
    {
        if (!m_config.hasVariable("viewport"))
        {
            m_builder.setViewport(renderer->getDefaultViewport())
                .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
            m_builder.setScissor(renderer->getDefaultScissor())
                .addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
            return;
        }

        const auto& viewportState = m_config["viewport"];
        {
            const auto& viewports = viewportState["viewports"];
            for (int i = 0; i < viewports.getLength(); ++i)
            {
                auto vp = viewports[0];
                if (vp.convertTo<std::string>() == "pass")
                    m_builder.setViewport(renderPass.createViewport());
            }
        }

        const auto& scissors = viewportState["scissors"];
        {
            for (int i = 0; i < scissors.getLength(); ++i)
            {
                const auto& scissor = scissors[0];
                if (scissor.convertTo<std::string>() == "pass")
                    m_builder.setScissor(renderPass.createScissor());
            }
        }
        /*if (auto viewports = viewport[)

        if (auto viewport = m_config.get<std::vector<float>>("viewport"); viewport && viewport.value().size() == 4)
        {
            const auto& vec = viewport.value();
            m_builder.setViewport(VkViewport{ vec[0], vec[1], vec[2], vec[3] });
        }
        else if (auto viewport = m_config.get<std::string>("viewport"); viewport == "pass")
        {
            m_builder.setViewport(renderPass.createViewport());
        }

        if (auto scissor = m_config.get<std::vector<int32_t>>("scissor"); scissor && scissor.value().size() == 4)
        {
            const auto& vec = scissor.value();
            m_builder.setScissor(VkRect2D{ { vec[0], vec[1] }, { static_cast<uint32_t>(vec[2]), static_cast<uint32_t>(vec[3]) } });
        }
        else if (auto scissor = m_config.get<std::string>("scissor"); scissor == "pass")
        {
            m_builder.setScissor(renderPass.createScissor());
        }*/
    }

    void LuaPipelineBuilder::readRasterizationState()
    {
        if (!m_config.hasVariable("rasterization"))
            return;

        const auto& rasterization = m_config["rasterization"];
        if (auto cullMode = rasterization.get<std::string>("cullMode"))
        {
            if (cullMode == "front")
                m_builder.setCullMode(VK_CULL_MODE_FRONT_BIT);
            else if (cullMode == "none")
                m_builder.setCullMode(VK_CULL_MODE_NONE);
        }

        if (auto polygonMode = rasterization.get<std::string>("polygonMode"))
        {
            if (polygonMode == "line")
                m_builder.setPolygonMode(VK_POLYGON_MODE_LINE);
            else if (polygonMode == "fill")
                m_builder.setPolygonMode(VK_POLYGON_MODE_FILL);
        }

        if (auto lineWidth = rasterization.get<float>("lineWidth"))
        {
            m_builder.setLineWidth(lineWidth.value());
        }
    }

    void LuaPipelineBuilder::readMultisampleState(const VulkanRenderPass& renderPass)
    {
        m_builder.setSampleCount(renderPass.getDefaultSampleCount());
        if (!m_config.hasVariable("multisample"))
            return;

        const auto& multisample = m_config["multisample"];

        if (auto enabled = multisample.get<bool>("alphaToCoverage"); enabled.has_value())
        {
            m_builder.setAlphaToCoverage(enabled.value());
        }
    }

    void LuaPipelineBuilder::readBlendState()
    {
        if (!m_config.hasVariable("colorBlend"))
            return;

        const auto& blend = m_config["colorBlend"];
        if (auto enabled = blend.get<bool>("enabled"); enabled.has_value())
        {
            m_builder.setBlendState(0, enabled.value());

            auto parseBlendFactor = [](const std::string& factorStr) {
                if (factorStr == "one") {
                    return VK_BLEND_FACTOR_ONE;
                }
                else if (factorStr == "oneMinusSrcAlpha") {
                    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                }
                else {
                    return VK_BLEND_FACTOR_MAX_ENUM;
                }
            };

            const VkBlendFactor srcBlend = parseBlendFactor(blend.get<std::string>("src").value_or("one"));
            const VkBlendFactor dstBlend = parseBlendFactor(blend.get<std::string>("dst").value_or("oneMinusSrcAlpha"));
            m_builder.setBlendFactors(0, srcBlend, dstBlend);
        }
    }

    void LuaPipelineBuilder::readDepthStencilState()
    {
        if (!m_config.hasVariable("depthStencil"))
            return;

        const auto& depthStencil = m_config["depthStencil"];
        if (auto reverseDepth = depthStencil.get<bool>("reverseDepth"); reverseDepth.has_value())
        {
            if (reverseDepth.value())
                m_builder.setDepthTestOperation(VK_COMPARE_OP_GREATER_OR_EQUAL);
        }

        if (auto depthWrite = depthStencil.get<bool>("depthWriteEnabled"); depthWrite.has_value())
        {
            if (depthWrite.value())
                m_builder.setDepthWrite(VK_TRUE);
            else
                m_builder.setDepthWrite(VK_FALSE);
        }

        if (auto depthTest = depthStencil.get<bool>("depthTest"); depthTest.has_value())
        {
            if (depthTest.value())
                m_builder.setDepthTest(depthTest.value());
        }
    }

    void LuaPipelineBuilder::readDescriptorSetBufferedStatus(PipelineLayoutBuilder& layoutBuilder)
    {
        if (m_config.hasVariable("setBuffering"))
        {
            auto setBufferingFlags = m_config.get<std::vector<bool>>("setBuffering");
            if (setBufferingFlags)
            {
                if (setBufferingFlags.value().size() != layoutBuilder.getDescriptorSetLayoutCount())
                    logger->warn("Mismatch in descriptor set count.");

                for (uint32_t i = 0; i < setBufferingFlags.value().size(); ++i)
                    layoutBuilder.setDescriptorSetBuffering(i, setBufferingFlags.value()[i]);
            }
        }
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
