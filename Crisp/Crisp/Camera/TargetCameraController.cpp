#include <Crisp/Camera/TargetCameraController.hpp>

namespace crisp {
TargetCameraController::TargetCameraController(Window& window)
    : m_window(&window)
    , m_camera(m_window->getSize().x, m_window->getSize().y)
    , m_target(0.0f)
    , m_distance(10.0f)
    , m_panSpeed(5.0f)
    , m_angularSpeed(glm::radians(90.0f))
    , m_yaw(0.0f)
    , m_pitch(0.0f)
    , m_isDragging(false)
    , m_prevMousePos(0.0f) {
    m_window->mouseButtonPressed.subscribe<&TargetCameraController::onMousePressed>(this);
    m_window->mouseButtonReleased.subscribe<&TargetCameraController::onMouseReleased>(this);
    m_window->mouseMoved.subscribe<&TargetCameraController::onMouseMoved>(this);
    m_window->mouseWheelScrolled.subscribe<&TargetCameraController::onMouseWheelScrolled>(this);

    updateOrientation(glm::radians(30.0f), -glm::radians(15.0f));
}

TargetCameraController::~TargetCameraController() {
    m_window->mouseButtonPressed.unsubscribe<&TargetCameraController::onMousePressed>(this);
    m_window->mouseButtonReleased.unsubscribe<&TargetCameraController::onMouseReleased>(this);
    m_window->mouseMoved.unsubscribe<&TargetCameraController::onMouseMoved>(this);
    m_window->mouseWheelScrolled.unsubscribe<&TargetCameraController::onMouseWheelScrolled>(this);
}

void TargetCameraController::setPanSpeed(const float panSpeed) {
    m_panSpeed = panSpeed;
}

void TargetCameraController::setTarget(const glm::vec3& target) {
    m_target = target;
    const glm::dquat orientation =
        glm::angleAxis(m_yaw, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_camera.setPosition(m_target + glm::quat(orientation) * glm::vec3(0.0f, 0.0f, m_distance));
    m_camera.setOrientation(orientation);
}

void TargetCameraController::setDistance(const float distance) {
    m_distance = distance;
    const glm::dquat orientation =
        glm::angleAxis(m_yaw, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_camera.setPosition(m_target + glm::quat(orientation) * glm::vec3(0.0f, 0.0f, m_distance));
    m_camera.setOrientation(orientation);
}

void TargetCameraController::setOrientation(float yaw, float pitch) {
    m_yaw = yaw;
    m_pitch = pitch;
    const glm::dquat orientation =
        glm::angleAxis(m_yaw, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_camera.setPosition(m_target + glm::quat(orientation) * glm::vec3(0.0f, 0.0f, m_distance));
    m_camera.setOrientation(orientation);
}

void TargetCameraController::pan(const float dx, const float dy) {
    m_target += -m_camera.getRightDir() * m_panSpeed * dx - m_camera.getUpDir() * m_panSpeed * dy;
    const glm::dquat orientation =
        glm::angleAxis(m_yaw, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_camera.setPosition(m_target + glm::quat(orientation) * glm::vec3(0.0f, 0.0f, m_distance));
    m_camera.setOrientation(orientation);
}

void TargetCameraController::updateOrientation(const float dYaw, const float dPitch) {
    m_yaw += m_angularSpeed * dYaw;
    m_pitch += m_angularSpeed * dPitch;
    const glm::dquat orientation =
        glm::angleAxis(m_yaw, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_camera.setPosition(m_target + glm::quat(orientation) * glm::vec3(0.0f, 0.0f, m_distance));
    m_camera.setOrientation(orientation);
}

const Camera& TargetCameraController::getCamera() const {
    return m_camera;
}

void TargetCameraController::update(const float /*dt*/) {}

void TargetCameraController::onMousePressed(const MouseEventArgs& mouseEventArgs) {
    if (mouseEventArgs.button == MouseButton::Left) {
        m_isDragging = true;
        m_window->setCursorState(CursorState::Disabled);

        m_prevMousePos.x = static_cast<float>(mouseEventArgs.x);
        m_prevMousePos.y = static_cast<float>(mouseEventArgs.y);
    }
}

void TargetCameraController::onMouseReleased(const MouseEventArgs& mouseEventArgs) {
    if (mouseEventArgs.button == MouseButton::Left) {
        m_isDragging = false;
        m_window->setCursorState(CursorState::Normal);

        m_prevMousePos.x = static_cast<float>(mouseEventArgs.x);
        m_prevMousePos.y = static_cast<float>(mouseEventArgs.y);
    }
}

void TargetCameraController::onMouseMoved(const double xPos, const double yPos) {
    const glm::vec2 mousePos(static_cast<float>(xPos), static_cast<float>(yPos));

    if (m_isDragging) {
        // In [-1, 1] range
        const auto delta = (mousePos - m_prevMousePos) / glm::vec2(m_window->getSize());

        if (m_window->isKeyDown(Key::LeftControl)) {
            pan(delta.x, -delta.y);
        } else {
            updateOrientation(-delta.x, -delta.y);
        }
    }

    m_prevMousePos = mousePos;
}

void TargetCameraController::onMouseWheelScrolled(const double offset) {
    if (m_window->isKeyDown(Key::LeftShift)) {
        const float distance = m_distance;
        setDistance(std::clamp(distance - static_cast<float>(offset) * 3.0f, 0.01f, 100.0f));
    } else {
        m_camera.setVerticalFov(std::clamp(m_camera.getVerticalFov() - static_cast<float>(offset) * 3.0f, 5.0f, 90.0f));
    }
}

void TargetCameraController::onViewportResized(int32_t width, int32_t height) {
    m_camera.setViewportSize(width, height);
}

CameraParameters TargetCameraController::getCameraParameters() const {
    CameraParameters params{};
    params.V = m_camera.getViewMatrix();
    params.P = m_camera.getProjectionMatrix();
    params.screenSize = glm::vec2(m_window->getSize());
    params.nearFar = m_camera.getViewDepthRange();
    return params;
}
} // namespace crisp