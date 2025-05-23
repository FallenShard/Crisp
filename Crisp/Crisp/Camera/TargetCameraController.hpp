#pragma once

#include <gsl/pointers>

#include <Crisp/Camera/Camera.hpp>

#include <Crisp/Core/Mouse.hpp>
#include <Crisp/Core/Window.hpp>

namespace crisp {
class TargetCameraController {
public:
    explicit TargetCameraController(Window& window);
    ~TargetCameraController();

    TargetCameraController(const TargetCameraController&) = default;
    TargetCameraController& operator=(const TargetCameraController&) = default;

    TargetCameraController(TargetCameraController&&) = default;
    TargetCameraController& operator=(TargetCameraController&&) = default;

    void setPanSpeed(float panSpeed);
    void setTarget(const glm::vec3& target);
    void setDistance(float distance);
    void setOrientation(float yaw, float pitch);

    void pan(float dx, float dy);
    void updateOrientation(float dYaw, float dPitch);

    const Camera& getCamera() const;

    void update(float dt);

    void onMousePressed(const MouseEventArgs& mouseEventArgs);
    void onMouseReleased(const MouseEventArgs& mouseEventArgs);
    void onMouseMoved(double xPos, double yPos);
    void onMouseWheelScrolled(double offset);

    void onViewportResized(int32_t width, int32_t height);

    CameraParameters getCameraParameters() const;

    float getDistance() const {
        return m_distance;
    }

    glm::vec3 getTarget() const {
        return m_target;
    }

    float getYaw() const {
        return glm::degrees(m_yaw);
    }

    float getPitch() const {
        return glm::degrees(m_pitch);
    }

private:
    gsl::not_null<Window*> m_window;
    Camera m_camera;

    glm::vec3 m_target;
    float m_distance;

    float m_panSpeed;
    float m_angularSpeed;

    float m_yaw;
    float m_pitch;

    bool m_isDraggingLeftClick{false};
    bool m_isDraggingRightClick{false};
    glm::vec2 m_prevMousePos;
};
} // namespace crisp