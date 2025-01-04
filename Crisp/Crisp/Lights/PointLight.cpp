#include <Crisp/Lights/PointLight.hpp>

namespace crisp {
namespace {
glm::mat4 invertProjectionY = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f));

} // namespace

PointLight::PointLight(const glm::vec3& power, const glm::vec3& position, const glm::vec3& directionHint)
    : m_power(power)
    , m_position(position, 1.0f) {
    m_view = glm::lookAt(glm::vec3(m_position), glm::normalize(directionHint), glm::vec3(0.0f, 1.0f, 0.0f));
    m_projection = invertProjectionY * glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 50.0f);
}

const glm::mat4& PointLight::getViewMatrix() const {
    return m_view;
}

const glm::mat4& PointLight::getProjectionMatrix() const {
    return m_projection;
}

glm::mat4 PointLight::createModelMatrix(float scale) const {
    return glm::translate(glm::vec3(m_position)) * glm::scale(glm::vec3(scale));
}

LightDescriptor PointLight::createDescriptorData() const {
    LightDescriptor data{};
    data.VP = m_projection * m_view;
    data.V = m_view;
    data.P = m_projection;
    data.position = m_position;
    data.spectrum = glm::vec4(m_power, 1.0f);
    data.params.r = m_radius;
    return data;
}
} // namespace crisp