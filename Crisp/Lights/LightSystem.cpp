#include "LightSystem.hpp"

#include "Renderer/UniformBuffer.hpp"
#include "vulkan/VulkanImage.hpp"

#include "Camera/AbstractCamera.hpp"
#include "Camera/CameraController.hpp"

#include "Techniques/ForwardClusteredShading.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph.hpp"

#include "Renderer/Pipelines/ComputePipeline.hpp"

#include <random>

namespace crisp
{
    namespace
    {
        static constexpr const char* LightCullingPass = "lightCullingPass";

        std::vector<float> angles;
        std::vector<float> velos;
        std::vector<float> radii;
        std::vector<glm::vec3> lightOrigins;
    }

    LightSystem::LightSystem(Renderer* renderer, uint32_t shadowMapSize)
        : m_renderer(renderer)
        , m_directionalLightBuffer(std::make_unique<UniformBuffer>(renderer, sizeof(LightDescriptor), BufferUpdatePolicy::PerFrame))
        , m_splitLambda(0.5f)
        , m_shadowMapSize(shadowMapSize)
    {
    }

    void LightSystem::enableCascadedShadowMapping(uint32_t cascadeCount, uint32_t shadowMapSize)
    {
        m_cascadedDirectionalLightBuffer = std::make_unique<UniformBuffer>(m_renderer, cascadeCount * sizeof(LightDescriptor), BufferUpdatePolicy::PerFrame);
        m_cascades.resize(cascadeCount);
        for (auto& cascade : m_cascades)
        {
            cascade.light  = std::make_unique<DirectionalLight>(*m_directionalLight);
            cascade.buffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(LightDescriptor), BufferUpdatePolicy::PerFrame);
        }
    }

    void LightSystem::update(const AbstractCamera& camera, float dt)
    {
        updateSplitIntervals(camera);
        for (uint32_t i = 0; i < m_cascades.size(); ++i)
        {
            auto& cascade = m_cascades[i];
            //cascade.light->fitProjectionToFrustum(camera.getFrustumPoints(cascade.begin, cascade.end));

            glm::vec4 centerRadius = camera.calculateBoundingSphere(cascade.begin, cascade.end);
            cascade.light->fitProjectionToFrustum(camera.getFrustumPoints(cascade.begin, cascade.end), centerRadius, centerRadius.w, m_shadowMapSize);

            const auto desc = cascade.light->createDescriptor();
            m_cascadedDirectionalLightBuffer->updateStagingBuffer(&desc, sizeof(LightDescriptor), i * sizeof(LightDescriptor));
            cascade.buffer->updateStagingBuffer(desc);
        }

        if (m_pointLightBuffer)
        {
            std::vector<LightDescriptor> lightDescriptors;
            lightDescriptors.reserve(m_pointLights.size());
            for (const auto& pl : m_pointLights)
                lightDescriptors.push_back(pl.createDescriptorData());
            //float w = 34 * 5.0f;
            //float h = 15.0f * 5.0f;
            //int r = 32;
            //int c = 32;
            //
            //for (int i = 0; i < r; ++i)
            //{
            //    float normI = static_cast<float>(i) / static_cast<float>(r - 1);
            //
            //    for (int j = 0; j < c; ++j)
            //    {
            //        float normJ = static_cast<float>(j) / static_cast<float>(c - 1);
            //        glm::vec3 position = glm::vec3((normJ - 0.5f) * w, 1.0f, (normI - 0.5f) * h);
            //        int index = i * c + j;
            //
            //        glm::vec4 rotPos = glm::vec4(radii[index], 0.0f, 0.0f, 1.0f);
            //        glm::mat4 rotation = glm::rotate(0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
            //        angles[index] += velos[index] * dt;
            //
            //        m_pointLights[index].setPosition(rotation * rotPos + glm::vec4(position, 1.0f));
            //        lightDescriptors.push_back(m_pointLights[index].createDescriptorData());
            //    }
            //}

            m_pointLightBuffer->updateStagingBuffer(lightDescriptors.data(), lightDescriptors.size() * sizeof(LightDescriptor));
        }
    }

    void LightSystem::setDirectionalLight(const DirectionalLight& dirLight)
    {
        if (!m_directionalLight)
            m_directionalLight = std::make_unique<DirectionalLight>(dirLight);
        else
            *m_directionalLight = dirLight;

        m_directionalLightBuffer->updateStagingBuffer(m_directionalLight->createDescriptor());
    }

    void LightSystem::setSplitLambda(float splitLambda)
    {
        m_splitLambda = splitLambda;
    }

    UniformBuffer* crisp::LightSystem::getDirectionalLightBuffer() const
    {
        return m_directionalLightBuffer.get();
    }

    UniformBuffer* LightSystem::getCascadedDirectionalLightBuffer() const
    {
        return m_cascadedDirectionalLightBuffer.get();
    }

    UniformBuffer* LightSystem::getCascadedDirectionalLightBuffer(uint32_t index) const
    {
        return m_cascades.at(index).buffer.get();
    }

    std::array<glm::vec3, 8> LightSystem::getCascadeFrustumPoints(uint32_t cascadeIndex) const
    {
        std::array<glm::vec3, 8> frustumPoints =
        {
            glm::vec3(-1.0f, -1.0f, 0.0f),
            glm::vec3(+1.0f, -1.0f, 0.0f),
            glm::vec3(+1.0f, +1.0f, 0.0f),
            glm::vec3(-1.0f, +1.0f, 0.0f),
            glm::vec3(-1.0f, -1.0f, 1.0f),
            glm::vec3(+1.0f, -1.0f, 1.0f),
            glm::vec3(+1.0f, +1.0f, 1.0f),
            glm::vec3(-1.0f, +1.0f, 1.0f)
        };

        auto lightToWorld = glm::inverse(m_cascades.at(cascadeIndex).light->createDescriptor().VP);
        for (auto& p : frustumPoints)
            p = glm::vec3(lightToWorld * glm::vec4(p, 1.0f));

        return frustumPoints;
    }

    float LightSystem::getCascadeSplitLo(uint32_t cascadeIndex) const
    {
        return m_cascades.at(cascadeIndex).begin;
    }

    float LightSystem::getCascadeSplitHi(uint32_t cascadeIndex) const
    {
        return m_cascades.at(cascadeIndex).end;
    }

    void LightSystem::createPointLightBuffer(uint32_t pointLightCount)
    {
        m_pointLights.resize(pointLightCount);

        std::vector<LightDescriptor> lightDescriptors;
        float w = 32 * 5.0f;
        float h = 15.0f * 5.0f;
        float zz = 15.0f * 5.0f;
        int r = 16;
        int c = 8;
        int l = 8;

        std::default_random_engine eng;
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (int i = 0; i < r; ++i)
        {
            float normI = static_cast<float>(i) / static_cast<float>(r - 1);

            for (int j = 0; j < c; ++j)
            {
                float normJ = static_cast<float>(j) / static_cast<float>(c - 1);
                for (int k = 0; k < l; ++k)
                {
                    float normK = static_cast<float>(k) / static_cast<float>(l - 1);
                    glm::vec3 spectrum = 5.0f * dist(eng) * glm::vec3(dist(eng), dist(eng), dist(eng));
                    glm::vec3 position = glm::vec3((normJ - 0.5f) * w, 1.0f + normK * zz, (normI - 0.5f) * h);
                    int index = i * c * l + j * l + k;

                    glm::vec3 pp = glm::vec3(dist(eng) - 0.5f, dist(eng), dist(eng) - 0.5f) * glm::vec3(w, zz, h);
                    m_pointLights[index] = PointLight(spectrum, pp, glm::vec3(1.0f));
                    m_pointLights[index].calculateRadius();
                    lightDescriptors.push_back(m_pointLights[index].createDescriptorData());

                }
            }
        }

        m_pointLightBuffer = std::make_unique<UniformBuffer>(m_renderer, m_pointLights.size() * sizeof(LightDescriptor), true, BufferUpdatePolicy::PerFrame, lightDescriptors.data());
    }

    void LightSystem::createTileGridBuffers(const CameraParameters& cameraParams)
    {
        m_tileSize = glm::ivec2(16); // 16x16 tiles
        m_tileGridDimensions = calculateTileGridDims(m_tileSize, cameraParams.screenSize);
        auto tileFrusta = createTileFrusta(m_tileSize, cameraParams.screenSize, cameraParams.P);
        std::size_t tileCount  = tileFrusta.size();

        m_tilePlaneBuffer       = std::make_unique<UniformBuffer>(m_renderer, tileCount * sizeof(TileFrustum), true, tileFrusta.data());
        m_lightIndexCountBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(uint32_t), true);
        m_lightIndexListBuffer  = std::make_unique<UniformBuffer>(m_renderer, tileCount * sizeof(uint32_t) * m_pointLights.size(), true);

        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.flags         = 0;
        createInfo.imageType     = VK_IMAGE_TYPE_2D;
        createInfo.format        = VK_FORMAT_R32G32_UINT;
        createInfo.extent        = VkExtent3D{ (uint32_t)m_tileGridDimensions.x, (uint32_t)m_tileGridDimensions.y, 1u };
        createInfo.mipLevels     = 1;
        createInfo.arrayLayers   = Renderer::NumVirtualFrames;
        createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_lightGrid = std::make_unique<VulkanImage>(m_renderer->getDevice(), createInfo, VK_IMAGE_ASPECT_COLOR_BIT);
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
            m_lightGridViews.emplace_back(m_lightGrid->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1));

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_lightGrid->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
    }

    void LightSystem::clusterLights(RenderGraph& renderGraph, const UniformBuffer& cameraBuffer)
    {
        auto& cullingPass = renderGraph.addComputePass(LightCullingPass);
        cullingPass.workGroupSize = glm::ivec3(m_tileSize, 1);
        cullingPass.numWorkGroups = glm::ivec3(m_tileGridDimensions, 1);
        cullingPass.pipeline = createLightCullingComputePipeline(m_renderer, cullingPass.workGroupSize);
        cullingPass.material = std::make_unique<Material>(cullingPass.pipeline.get());
        cullingPass.material->writeDescriptor(0, 0, m_tilePlaneBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(0, 1, m_lightIndexCountBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(0, 2, m_pointLightBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(0, 3, cameraBuffer.getDescriptorInfo());
        cullingPass.material->writeDescriptor(0, 4, m_lightIndexListBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(1, 0, m_lightGridViews, nullptr, VK_IMAGE_LAYOUT_GENERAL );
        cullingPass.material->setDynamicBufferView(0, *m_lightIndexCountBuffer, 0);
        cullingPass.material->setDynamicBufferView(1, *m_pointLightBuffer, 0);
        cullingPass.material->setDynamicBufferView(2, cameraBuffer, 0);
        cullingPass.material->setDynamicBufferView(3, *m_lightIndexListBuffer, 0);
        cullingPass.preDispatchCallback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            // Before culling can start, zero out the light index count buffer
            glm::uvec4 zero(0);

            VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.buffer        = m_lightIndexCountBuffer->get();
            barrier.offset        = frameIndex * sizeof(zero);
            barrier.size          = sizeof(zero);

            vkCmdUpdateBuffer(cmdBuffer, barrier.buffer, barrier.offset, barrier.size, &zero);
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 1, &barrier, 0, nullptr);
        };

        //m_renderGraph->addRenderTargetLayoutTransition(DepthPrePass, LightCullingPass, 0);
        renderGraph.addDependency(LightCullingPass, "MainPass", [this](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            auto info = m_lightIndexListBuffer->getDescriptorInfo();
            barrier.buffer        = info.buffer;
            barrier.offset        = info.offset;
            barrier.size          = info.range;

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 1, &barrier, 0, nullptr);
        });

        // TODO: Unsorted

        //// Point shadow map
        //auto smPipeline = m_renderer->createPipelineFromLua("ShadowMap.lua", shadowPassNode.renderPass.get(), 0);
        //auto smMaterial = std::make_unique<Material>(smPipeline.get());
        //smMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //smMaterial->writeDescriptor(0, 1, m_shadowMapper->getLightTransformBuffer()->getDescriptorInfo());
        //smMaterial->setDynamicBufferView(1, *m_shadowMapper->getLightTransformBuffer(), 0);
        //m_pipelines.emplace("sm", std::move(smPipeline));
        //m_materials.emplace("sm", std::move(smMaterial));

        //auto vsmPipeline = m_renderer->createPipelineFromLua("VarianceShadowMap.lua", vsmPassNode.renderPass.get(), 0);
        //auto vsmMaterial = std::make_unique<Material>(vsmPipeline.get());
        //vsmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //vsmMaterial->writeDescriptor(0, 1, m_shadowMapper->getLightFullTransformBuffer()->getDescriptorInfo());
        //vsmMaterial->setDynamicBufferView(1, *m_shadowMapper->getLightFullTransformBuffer(), 0);
        //m_pipelines.emplace("vsm", std::move(vsmPipeline));
        //m_materials.emplace("vsm", std::move(vsmMaterial));

        //auto depthPipeline = m_renderer->createPipelineFromLua("DepthPrepass.lua", depthPrePass.renderPass.get(), 0);
        //auto depthMaterial = std::make_unique<Material>(depthPipeline.get());
        //depthMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //m_pipelines.emplace("depth", std::move(depthPipeline));
        //m_materials.emplace("depth", std::move(depthMaterial));
    }

    UniformBuffer* LightSystem::getPointLightBuffer() const
    {
        return m_pointLightBuffer.get();
    }

    UniformBuffer* LightSystem::getLightIndexBuffer() const
    {
        return m_lightIndexListBuffer.get();;
    }

    const std::vector<std::unique_ptr<VulkanImageView>>& LightSystem::getTileGridViews() const
    {
        return m_lightGridViews;
    }

    void LightSystem::updateSplitIntervals(const AbstractCamera& camera)
    {
        if (m_cascades.empty())
            return;

        float zFar = camera.getFarPlaneDistance();
        float zNear = camera.getNearPlaneDistance();
        float range = zFar - zNear;
        float ratio = zFar / zNear;

        m_cascades[0].begin = zNear;
        m_cascades[m_cascades.size() - 1].end = zFar;
        for (uint32_t i = 0; i < m_cascades.size() - 1; i++)
        {
            float p = (i + 1) / static_cast<float>(m_cascades.size());
            float logSplit = zNear * std::pow(ratio, p);
            float linSplit = zNear + range * p;
            float splitPos = m_splitLambda * (logSplit - linSplit) + linSplit;
            m_cascades[i].end       = splitPos - zNear;
            m_cascades[i + 1].begin = splitPos - zNear;
        }
    }
}
