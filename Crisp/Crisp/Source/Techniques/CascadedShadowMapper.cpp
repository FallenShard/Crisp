#include "CascadedShadowMapper.hpp"

#include "Camera/AbstractCamera.hpp"
#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/Pipelines/ShadowMapPipeline.hpp"

namespace crisp
{
    CascadedShadowMapper::CascadedShadowMapper(VulkanRenderer* renderer, DirectionalLight light, uint32_t numCascades, UniformBuffer* modelTransformsBuffer)
        : m_renderer(renderer)
        , m_numCascades(numCascades)
        , m_light(light)
    {
        m_shadowPass = std::make_unique<ShadowPass>(m_renderer, 2048, m_numCascades);
        m_pipelines.reserve(m_numCascades);
        for (uint32_t i = 0; i < m_numCascades; i++)
            m_pipelines.emplace_back(std::make_unique<ShadowMapPipeline>(m_renderer, m_shadowPass.get(), i));
        
        m_descGroup =
        {
            m_pipelines[0]->allocateDescriptorSet(0)
        };

        m_transforms.resize(m_numCascades);
        m_transformsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_numCascades * sizeof(glm::mat4), BufferUpdatePolicy::PerFrame, m_transforms.data());

        m_descGroup.postBufferUpdate(0, 0, { modelTransformsBuffer->get(), 0, 4 * sizeof(glm::mat4) });
        m_descGroup.postBufferUpdate(0, 1, m_transformsBuffer->getDescriptorInfo());
        m_descGroup.flushUpdates(m_renderer->getDevice());
    }

    CascadedShadowMapper::~CascadedShadowMapper()
    {
    }

    UniformBuffer* CascadedShadowMapper::getLightTransformsBuffer() const
    {
        return m_transformsBuffer.get();
    }

    ShadowPass* CascadedShadowMapper::getRenderPass() const
    {
        return m_shadowPass.get();
    }

    void CascadedShadowMapper::recalculateLightProjections(const AbstractCamera& camera, float zFar, ParallelSplit splitStrategy)
    {
        auto zNear = camera.getNearPlaneDistance();
        std::vector<float> splitsNear = { zNear, 5.0f, 10.0f, 20.0f };
        std::vector<float> splitsFar  = { 5.0f, 10.0f, 20.0f, zFar };

        for (uint32_t i = 0; i < m_numCascades; i++)
        {
            m_light.calculateProjection(camera.getFrustumPoints(splitsNear[i], splitsFar[i]));
            m_transforms[i] = m_light.getProjectionMatrix() * m_light.getViewMatrix();
        }

        m_transformsBuffer->updateStagingBuffer(m_transforms.data(), m_numCascades * sizeof(glm::mat4));
    }

    void CascadedShadowMapper::update(VkCommandBuffer cmdBuffer, unsigned int frameIndex)
    {
        m_transformsBuffer->updateDeviceBuffer(cmdBuffer, frameIndex);
    }

    void CascadedShadowMapper::draw(VkCommandBuffer commandBuffer, unsigned int frameIndex, std::function<void(VkCommandBuffer commandBuffer, uint32_t frameIdx, DescriptorSetGroup& descSets, VulkanPipeline* pipeline, uint32_t cascadeTransformOffset)> callback)
    {
        m_descGroup.setDynamicOffset(1, m_transformsBuffer->getDynamicOffset(frameIndex));

        m_shadowPass->begin(commandBuffer);
        callback(commandBuffer, frameIndex, m_descGroup, m_pipelines[0].get(), 0);

        for (uint32_t i = 1; i < m_numCascades; i++)
        {
            vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
            callback(commandBuffer, frameIndex, m_descGroup, m_pipelines[i].get(), i);
        }

        m_shadowPass->end(commandBuffer);
    }

    glm::mat4 CascadedShadowMapper::getLightTransform(uint32_t index) const
    {
        return m_transforms[index];
    }

    const DirectionalLight* CascadedShadowMapper::getLight() const
    {
        return &m_light;
    }

    VkImage CascadedShadowMapper::getShadowMap(int idx)
    {
        return m_shadowPass->getColorAttachment(idx);
    }
}




