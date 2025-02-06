#include <Crisp/Geometry/TransformBuffer.hpp>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
TransformBuffer::TransformBuffer(Renderer* renderer, const std::size_t maxTransformCount)
    : m_activeTransforms(0)
    , m_transforms(maxTransformCount)
    , m_transformBuffer(createUniformRingBuffer(&renderer->getDevice(), m_transforms.size() * sizeof(TransformPack))) {
    renderer->getDebugMarker().setObjectName(m_transformBuffer->getHandle(), "transformBuffer");
}

const VulkanRingBuffer* TransformBuffer::getUniformBuffer() const {
    return m_transformBuffer.get();
}

VulkanRingBuffer* TransformBuffer::getUniformBuffer() {
    return m_transformBuffer.get();
}

TransformPack& TransformBuffer::getPack(TransformHandle handle) {
    return m_transforms.at(handle.index);
}

void TransformBuffer::update(const glm::mat4& V, const glm::mat4& P) {
    for (uint32_t i = 0; i < m_activeTransforms; ++i) {
        auto& trans = m_transforms[i];
        trans.MV = V * trans.M;
        trans.MVP = P * trans.MV;
        trans.N = glm::transpose(glm::inverse(glm::mat3(trans.MV)));
    }
}

void TransformBuffer::updateStagingBuffer(const uint32_t regionIndex) {
    m_transformBuffer->updateStagingBuffer(
        {
            .data = m_transforms.data(),
            .size = m_activeTransforms * sizeof(TransformPack),
        },
        regionIndex);
}

TransformHandle TransformBuffer::getNextIndex() {
    CRISP_CHECK(m_activeTransforms < m_transforms.size());
    return TransformHandle{{{static_cast<uint16_t>(m_activeTransforms++), 0}}};
}
} // namespace crisp
