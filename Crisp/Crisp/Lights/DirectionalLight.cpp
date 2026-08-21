#include <Crisp/Lights/DirectionalLight.hpp>

#include <cmath>
#include <limits>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
namespace {
constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};

glm::vec3 normalizeDirection(const glm::vec3& direction) {
    CRISP_CHECK(
        std::isfinite(direction.x) && std::isfinite(direction.y) && std::isfinite(direction.z),
        "A directional light direction must be finite.");

    const float lengthSquared = glm::length2(direction);
    CRISP_CHECK(std::isfinite(lengthSquared) && lengthSquared > 0.0f, "A directional light direction must be non-zero.");
    return direction / std::sqrt(lengthSquared);
}

struct LightBasis {
    glm::vec3 right;
    glm::vec3 up;
};

glm::vec3 getLeastAlignedAxis(const glm::vec3& direction) {
    const glm::vec3 absDirection = glm::abs(direction);
    if (absDirection.x <= absDirection.y && absDirection.x <= absDirection.z) {
        return {1.0f, 0.0f, 0.0f};
    }
    if (absDirection.y <= absDirection.z) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {0.0f, 0.0f, 1.0f};
}

LightBasis calculateLightBasis(const glm::vec3& direction, const glm::vec3& preferredUp) {
    glm::vec3 up = preferredUp - direction * glm::dot(preferredUp, direction);
    if (glm::length2(up) <= std::numeric_limits<float>::epsilon()) {
        const glm::vec3 referenceAxis = getLeastAlignedAxis(direction);
        up = referenceAxis - direction * glm::dot(referenceAxis, direction);
    }

    up = glm::normalize(up);
    const glm::vec3 right = glm::normalize(glm::cross(direction, up));
    return {.right = right, .up = glm::normalize(glm::cross(right, direction))};
}

glm::mat4 calculateViewMatrix(
    const glm::vec3& direction, const glm::vec3& up, const glm::vec3& origin = glm::vec3(0.0f)) {
    return glm::lookAt(origin, origin + direction, up);
}
} // namespace

DirectionalLight::DirectionalLight()
    : DirectionalLight(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(-1.0f), glm::vec3(1.0f)) {}

DirectionalLight::DirectionalLight(
    const glm::vec3& direction, const glm::vec3& radiance, const glm::vec3& extentMin, const glm::vec3& extentMax)
    : m_direction(normalizeDirection(direction))
    , m_radiance(radiance)
    , m_up(calculateLightBasis(m_direction, kWorldUp).up)
    , m_view(calculateViewMatrix(m_direction, m_up))
    , m_projection{glm::ortho(extentMin.x, extentMax.x, extentMin.y, extentMax.y, extentMin.z, extentMax.z)} {}

void DirectionalLight::setDirection(glm::vec3 direction) {
    const glm::vec3 normalizedDirection = normalizeDirection(direction);
    m_up = calculateLightBasis(normalizedDirection, m_up).up;
    m_direction = normalizedDirection;
    m_view = calculateViewMatrix(m_direction, m_up);
}

const glm::vec3& DirectionalLight::getDirection() const {
    return m_direction;
}

const glm::mat4& DirectionalLight::getViewMatrix() const {
    return m_view;
}

const glm::mat4& DirectionalLight::getProjectionMatrix() const {
    return m_projection;
}

void DirectionalLight::fitProjectionToBoundingSphere(
    const glm::vec3& center, const float radius, const uint32_t shadowMapSize) {
    CRISP_CHECK(
        std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(center.z),
        "The cascade bounding-sphere center must be finite.");
    CRISP_CHECK(std::isfinite(radius) && radius > 0.0f, "The cascade bounding-sphere radius must be positive.");

    constexpr uint32_t kGuardTexelCount{1};
    CRISP_CHECK_GT(shadowMapSize, 2 * kGuardTexelCount, "The shadow map must have room for the guard band.");

    const auto [right, up] = calculateLightBasis(m_direction, m_up);
    const auto mapResolution = static_cast<float>(shadowMapSize);

    // A fixed guard band keeps the receiver sphere inside the projection after snapping its center.
    const float xyExtent = radius / (1.0f - 2.0f * static_cast<float>(kGuardTexelCount) / mapResolution);
    const float worldUnitsPerTexel = 2.0f * xyExtent / mapResolution;
    const float snappedRight = std::round(glm::dot(center, right) / worldUnitsPerTexel) * worldUnitsPerTexel;
    const float snappedUp = std::round(glm::dot(center, up) / worldUnitsPerTexel) * worldUnitsPerTexel;
    const glm::vec3 snappedCenter = right * snappedRight + up * snappedUp + m_direction * glm::dot(center, m_direction);

    const glm::vec3 origin = snappedCenter - m_direction * radius;
    m_view = calculateViewMatrix(m_direction, up, origin);
    m_projection = glm::ortho(-xyExtent, xyExtent, -xyExtent, xyExtent, 0.0f, 2.0f * radius);
}

LightDescriptor DirectionalLight::createDescriptor() const {
    LightDescriptor desc = {};
    desc.V = m_view;
    desc.P = m_projection;
    desc.VP = m_projection * m_view;
    desc.direction = glm::vec4(-m_direction, 0.0f);
    desc.spectrum = glm::vec4(m_radiance, 1.0f);
    return desc;
}
} // namespace crisp
