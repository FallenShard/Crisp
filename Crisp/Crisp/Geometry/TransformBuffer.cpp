#include <Crisp/Geometry/TransformBuffer.hpp>

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace crisp
{
    TransformBuffer::TransformBuffer(Renderer* renderer, std::size_t numObjects)
        : m_transforms(numObjects)
        , m_transformBuffer(std::make_unique<UniformBuffer>(renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame))
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

    TransformPack* TransformBuffer::getPack(int index)
    {
        return &m_transforms.at(index);
    }

    void TransformBuffer::update(const glm::mat4& V, const glm::mat4& P)
    {
        if (m_transforms.size() > 500)
        {
            m_threadPool.parallelFor(m_transforms.size(), [this, &V, &P](int i, int jobIdx) {
                    auto& trans = m_transforms[i];
                    trans.MV = V * trans.M;
                    trans.MVP = P * trans.MV;
                    trans.N = glm::transpose(glm::inverse(glm::mat3(trans.MV)));
                });
        }
        else
        {
            for (auto& trans : m_transforms)
            {
                trans.MV = V * trans.M;
                trans.MVP = P * trans.MV;
                trans.N = glm::transpose(glm::inverse(glm::mat3(trans.MV)));
            }
        }

        m_transformBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(TransformPack));
    }
}
