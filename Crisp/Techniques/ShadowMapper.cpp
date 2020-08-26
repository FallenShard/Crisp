#include "ShadowMapper.hpp"

#include "Renderer/UniformBuffer.hpp"

namespace crisp
{
    ShadowMapper::ShadowMapper(Renderer* renderer, uint32_t numLights)
        : m_lightTransforms(numLights)
        , m_lightTransformBuffer(std::make_unique<UniformBuffer>(renderer, numLights * sizeof(glm::mat4), BufferUpdatePolicy::PerFrame))
        , m_lightFullTransforms(numLights)
        , m_lightFullTransformBuffer(std::make_unique<UniformBuffer>(renderer, numLights * sizeof(glm::mat4) * 2, BufferUpdatePolicy::PerFrame))
    {
    }

    ShadowMapper::~ShadowMapper()
    {
    }

    void ShadowMapper::setLightTransform(uint32_t lightIndex, const glm::mat4& transform)
    {
        m_lightTransforms[lightIndex] = transform;
        m_lightTransformBuffer->updateStagingBuffer(m_lightTransforms.data(), m_lightTransforms.size() * sizeof(glm::mat4));
    }

    void ShadowMapper::setLightFullTransform(uint32_t lightIndex, const glm::mat4& view, const glm::mat4& projection)
    {
        m_lightFullTransforms[2 * lightIndex + 0] = view;
        m_lightFullTransforms[2 * lightIndex + 1] = projection;
        m_lightFullTransformBuffer->updateStagingBuffer(m_lightFullTransforms.data(), m_lightFullTransforms.size() * sizeof(glm::mat4));
    }

    UniformBuffer* ShadowMapper::getLightTransformBuffer() const
    {
        return m_lightTransformBuffer.get();
    }

    UniformBuffer* ShadowMapper::getLightFullTransformBuffer() const
    {
        return m_lightFullTransformBuffer.get();
    }
}

