#include <Crisp/Camera/Camera.hpp>

namespace
{
constexpr float kDefaultNearZ{0.1f};
constexpr float kDefaultFarZ{1000.0f};
const glm::mat4 InvertProjectionY = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f));

glm::mat4 reverseZPerspective(float fovY, float aspectRatio, float zNear, float zFar)
{
    const glm::mat4 test = glm::perspective(fovY, aspectRatio, zNear, 500.0f);
    const float f = 1.0f / std::tan(fovY / 2.0f);
    const auto a = glm::mat4(
        f / aspectRatio,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        -zFar / (zNear - zFar) - 1,
        -1.0f,
        0.0f,
        0.0f,
        -zNear * zFar / (zNear - zFar),
        0.0f);

    const auto b = glm::infinitePerspective(fovY, aspectRatio, zNear);

    // Infinite projection with z value projected into [0, 1].
    const auto c = glm::mat4(
        f / aspectRatio, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, zNear, 0.0f);
    return c;
}

template <typename ScalarType>
constexpr glm::vec<3, ScalarType> kAxisX = {ScalarType(1.0), ScalarType(0.0), ScalarType(0.0)};
template <typename ScalarType>
constexpr glm::vec<3, ScalarType> kAxisY = {ScalarType(0.0), ScalarType(1.0), ScalarType(0.0)};
template <typename ScalarType>
constexpr glm::vec<3, ScalarType> kAxisZ = {ScalarType(0.0), ScalarType(0.0), ScalarType(1.0)};
} // namespace

namespace crisp
{
Camera::Camera(const int32_t viewportWidth, const int32_t viewportHeight)
    : Camera(viewportWidth, viewportHeight, kDefaultNearZ, kDefaultFarZ)
{
}

Camera::Camera(const int32_t viewportWidth, const int32_t viewportHeight, const float zNear, const float zFar)
    : m_verticalFov(glm::radians(45.0f))
    , m_aspectRatio(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight))
    , m_zNear(zNear)
    , m_zFar(zFar)
    , m_position(0.0f, 0.0f, 10.0f)
    , m_orientation(glm::angleAxis(glm::radians(0.0), kAxisY<double>))
{
    updateProjectionMatrix();
    updateViewMatrix();
}

void Camera::setVerticalFov(const float verticalFov)
{
    m_verticalFov = glm::radians(verticalFov);
    updateProjectionMatrix();
}

float Camera::getVerticalFov() const
{
    return glm::degrees(m_verticalFov);
}

void Camera::setViewportSize(const int32_t viewportWidth, const int32_t viewportHeight)
{
    setAspectRatio(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight));
}

void Camera::setAspectRatio(const float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    updateProjectionMatrix();
}

glm::mat4 Camera::getProjectionMatrix() const
{
    return m_P;
}

void Camera::setPosition(const glm::vec3& position)
{
    m_position = position;
    updateViewMatrix();
}

void Camera::translate(const glm::vec3& translation)
{
    m_position += translation;
    updateViewMatrix();
}

glm::vec3 Camera::getPosition() const
{
    return m_position;
}

void Camera::setOrientation(const glm::dquat& orientation)
{
    m_orientation = orientation;
    updateViewMatrix();
}

glm::mat4 Camera::getViewMatrix() const
{
    return m_V;
}

glm::mat4 Camera::getInvViewMatrix() const
{
    return m_invV;
}

glm::vec3 Camera::getRightDir() const
{
    return m_invV * glm::vec4(kAxisX<float>, 0.0f);
}

glm::vec3 Camera::getLookDir() const
{
    return m_invV * glm::vec4(-kAxisZ<float>, 0.0f);
}

glm::vec3 Camera::getUpDir() const
{
    return m_invV * glm::vec4(kAxisY<float>, 0.0f);
}

glm::vec2 Camera::getViewDepthRange() const
{
    return glm::vec2(m_zNear, m_zFar);
}

std::array<glm::vec3, Camera::kFrustumPointCount> Camera::computeFrustumPoints(
    const float zNear, const float zFar) const
{
    const float tanFovHalf = std::tan(m_verticalFov * 0.5f);

    const float halfNearH = zNear * tanFovHalf;
    const float halfNearW = halfNearH * m_aspectRatio;

    const float halfFarH = zFar * tanFovHalf;
    const float halfFarW = halfFarH * m_aspectRatio;

    std::array<glm::vec3, kFrustumPointCount> frustumPoints = {
        glm::vec3(-halfNearW, -halfNearH, -zNear),
        glm::vec3(+halfNearW, -halfNearH, -zNear),
        glm::vec3(+halfNearW, +halfNearH, -zNear),
        glm::vec3(-halfNearW, +halfNearH, -zNear),
        glm::vec3(-halfFarW, -halfFarH, -zFar),
        glm::vec3(+halfFarW, -halfFarH, -zFar),
        glm::vec3(+halfFarW, +halfFarH, -zFar),
        glm::vec3(-halfFarW, +halfFarH, -zFar)};

    for (auto& p : frustumPoints)
        p = glm::vec3(m_invV * glm::vec4(p, 1.0f));

    return frustumPoints;
}

std::array<glm::vec3, Camera::kFrustumPointCount> Camera::computeFrustumPoints() const
{
    return computeFrustumPoints(m_zNear, m_zFar);
}

glm::vec4 Camera::computeFrustumBoundingSphere(const float zNear, const float zFar) const
{
    const float k = std::sqrt(1.0f + m_aspectRatio * m_aspectRatio) * std::tan(m_verticalFov * 0.5f);
    if (k * k >= (zFar - zNear) / (zFar + zNear))
    {
        const glm::vec3 center = m_invV * glm::vec4(0.0f, 0.0f, -zFar, 1.0f);
        return glm::vec4(center, zFar * k);
    }
    else
    {
        const glm::vec3 center = m_invV * glm::vec4(0.0f, 0.0f, -0.5f * (zFar + zNear) * (1 + k * k), 1.0f);
        const float radius = 0.5f * std::sqrt(
                                        (zFar - zNear) * (zFar - zNear) + 2.0f * (zFar * zFar + zNear * zNear) * k * k +
                                        (zFar + zNear) * (zFar + zNear) * k * k * k * k);
        return glm::vec4(center, radius);
    }
}

void Camera::updateProjectionMatrix()
{
    m_P = InvertProjectionY * reverseZPerspective(m_verticalFov, m_aspectRatio, m_zNear, m_zFar);
}

void Camera::updateViewMatrix()
{
    const glm::mat4 rotationMat = glm::mat4_cast(m_orientation);
    m_V = glm::transpose(rotationMat) * glm::translate(-m_position);
    m_invV = glm::translate(m_position) * rotationMat;
}
} // namespace crisp