#include <Crisp/Camera/TargetCameraController.hpp>

#include <algorithm>
#include <cmath>

namespace crisp {
TargetCameraController::TargetCameraController(Window& window)
    : m_window(&window)
    , m_camera(m_window->getSize().x, m_window->getSize().y)
    , m_target(0.0f)
    , m_distance(10.0f)
    , m_panSpeed(5.0f)
    , m_angularSpeed(glm::radians(0.15f))
    , m_yaw(0.0f)
    , m_pitch(0.0f)
    , m_prevMousePos(0.0f) {
    m_window->mouseButtonPressed.subscribe<&TargetCameraController::onMousePressed>(this);
    m_window->mouseButtonReleased.subscribe<&TargetCameraController::onMouseReleased>(this);
    m_window->mouseMoved.subscribe<&TargetCameraController::onMouseMoved>(this);
    m_window->mouseWheelScrolled.subscribe<&TargetCameraController::onMouseWheelScrolled>(this);

    setOrientation(glm::radians(30.0f), -glm::radians(15.0f));
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
    m_distance = std::max(distance, 0.01f);
    const glm::dquat orientation =
        glm::angleAxis(m_yaw, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_camera.setPosition(m_target + glm::quat(orientation) * glm::vec3(0.0f, 0.0f, m_distance));
    m_camera.setOrientation(orientation);
}

void TargetCameraController::setOrbitDistance(const float distance) {
    m_distance = std::max(distance, 0.01f);
    m_target = m_camera.getPosition() + m_camera.getLookDir() * m_distance;
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
    const float viewportHeight = std::max(static_cast<float>(m_window->getSize().y), 1.0f);
    const float verticalFov = glm::radians(m_camera.getVerticalFov());
    const float worldUnitsPerPixel = 2.0f * m_distance * std::tan(verticalFov * 0.5f) / viewportHeight;
    m_target += (-m_camera.getRightDir() * dx - m_camera.getUpDir() * dy) * worldUnitsPerPixel;
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

void TargetCameraController::look(const float dYaw, const float dPitch) {
    m_yaw += m_angularSpeed * dYaw;
    m_pitch += m_angularSpeed * dPitch;
    const glm::dquat orientation =
        glm::angleAxis(m_yaw, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_camera.setOrientation(orientation);
    m_target = m_camera.getPosition() + m_camera.getLookDir() * m_distance;
}

const Camera& TargetCameraController::getCamera() const {
    return m_camera;
}

void TargetCameraController::update(const float dt) {
    glm::vec3 moveDirection{0.0f};
    if (m_window->isKeyDown(Key::A)) {
        moveDirection -= m_camera.getRightDir();
    }
    if (m_window->isKeyDown(Key::D)) {
        moveDirection += m_camera.getRightDir();
    }
    if (m_window->isKeyDown(Key::W)) {
        moveDirection += m_camera.getLookDir();
    }
    if (m_window->isKeyDown(Key::S)) {
        moveDirection -= m_camera.getLookDir();
    }

    if (glm::dot(moveDirection, moveDirection) > 0.0f) {
        const glm::vec3 translation = glm::normalize(moveDirection) * m_panSpeed * dt;
        m_camera.translate(translation);
        m_target += translation;
    }
}

void TargetCameraController::onMousePressed(const MouseEventArgs& mouseEventArgs) {
    if (m_dragMode != DragMode::None) {
        return;
    }

    const bool isControlDown = static_cast<bool>(mouseEventArgs.modifiers & Modifier::Control) ||
        m_window->isKeyDown(Key::LeftControl) || m_window->isKeyDown(Key::RightControl);
    if (isControlDown && mouseEventArgs.button == MouseButton::Left) {
        m_dragMode = DragMode::Orbit;
    } else if (isControlDown && mouseEventArgs.button == MouseButton::Right) {
        m_dragMode = DragMode::Pan;
    } else if (!isControlDown && mouseEventArgs.button == MouseButton::Right) {
        m_dragMode = DragMode::Look;
    } else {
        return;
    }

    m_window->setCursorState(CursorState::Disabled);
    m_prevMousePos.x = static_cast<float>(mouseEventArgs.x);
    m_prevMousePos.y = static_cast<float>(mouseEventArgs.y);
}

void TargetCameraController::onMouseReleased(const MouseEventArgs& mouseEventArgs) {
    const bool releasedActiveButton =
        (mouseEventArgs.button == MouseButton::Left && m_dragMode == DragMode::Orbit) ||
        (mouseEventArgs.button == MouseButton::Right &&
         (m_dragMode == DragMode::Look || m_dragMode == DragMode::Pan));
    if (!releasedActiveButton) {
        return;
    }

    m_dragMode = DragMode::None;
    m_window->setCursorState(CursorState::Normal);
    m_prevMousePos.x = static_cast<float>(mouseEventArgs.x);
    m_prevMousePos.y = static_cast<float>(mouseEventArgs.y);
}

void TargetCameraController::onMouseMoved(const double xPos, const double yPos) {
    const glm::vec2 mousePos(static_cast<float>(xPos), static_cast<float>(yPos));

    const glm::vec2 delta = mousePos - m_prevMousePos;
    if (m_dragMode == DragMode::Look) {
        look(-delta.x, -delta.y);
    } else if (m_dragMode == DragMode::Orbit) {
        updateOrientation(-delta.x, -delta.y);
    } else if (m_dragMode == DragMode::Pan) {
        pan(delta.x, -delta.y);
    }

    m_prevMousePos = mousePos;
}

void TargetCameraController::onMouseWheelScrolled(const double offset) {
    const bool isControlDown =
        m_window->isKeyDown(Key::LeftControl) || m_window->isKeyDown(Key::RightControl);
    if (isControlDown) {
        const float scale = std::pow(0.9f, static_cast<float>(offset));
        setDistance(std::clamp(m_distance * scale, 0.01f, 100.0f));
    } else {
        const glm::vec3 translation =
            m_camera.getLookDir() * static_cast<float>(offset) * m_panSpeed * 0.2f;
        m_camera.translate(translation);
        m_target += translation;
    }
}

void TargetCameraController::onViewportResized(int32_t width, int32_t height) {
    m_camera.setViewportSize(width, height);
}

CameraParameters TargetCameraController::getCameraParameters() const {
    CameraParameters params{};
    params.V = m_camera.getViewMatrix();
    params.P = m_camera.getProjectionMatrix();
    params.invV = glm::inverse(params.V);
    params.invP = glm::inverse(params.P);
    params.screenSize = glm::vec2(m_window->getSize());
    params.nearFar = m_camera.getViewDepthRange();
    return params;
}
} // namespace crisp
