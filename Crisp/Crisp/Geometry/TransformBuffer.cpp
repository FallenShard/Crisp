#include <Crisp/Geometry/TransformBuffer.hpp>

#include <Crisp/Common/Checks.hpp>

namespace crisp
{
TransformBuffer::TransformBuffer(Renderer* renderer, const std::size_t maxTransformCount)
    : m_activeTransforms(0)
    , m_transforms(maxTransformCount)
    , m_transformBuffer(std::make_unique<UniformBuffer>(
          renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame))
{
}

const UniformBuffer* TransformBuffer::getUniformBuffer() const
{
    return m_transformBuffer.get();
}

UniformBuffer* TransformBuffer::getUniformBuffer()
{
    return m_transformBuffer.get();
}

TransformPack& TransformBuffer::getPack(TransformHandle handle)
{
    return m_transforms.at(handle.index);
}

void TransformBuffer::update(const glm::mat4& V, const glm::mat4& P)
{
    if (m_activeTransforms > 500)
    {
        m_threadPool.parallelFor(
            m_activeTransforms,
            [this, &V, &P](const std::size_t i, const std::size_t /*jobIdx*/)
            {
                auto& trans = m_transforms[i];
                trans.MV = V * trans.M;
                trans.MVP = P * trans.MV;
                trans.N = glm::transpose(glm::inverse(glm::mat3(trans.MV)));
            });
    }
    else
    {
        for (uint32_t i = 0; i < m_activeTransforms; ++i)
        {
            auto& trans = m_transforms[i];
            trans.MV = V * trans.M;
            trans.MVP = P * trans.MV;
            trans.N = glm::transpose(glm::inverse(glm::mat3(trans.MV)));
        }
    }

    m_transformBuffer->updateStagingBuffer(m_transforms.data(), m_activeTransforms * sizeof(TransformPack));
}

TransformHandle TransformBuffer::getNextIndex()
{
    CRISP_CHECK(m_activeTransforms < m_transforms.size());
    return TransformHandle{static_cast<uint16_t>(m_activeTransforms++), 0};
}
} // namespace crisp
