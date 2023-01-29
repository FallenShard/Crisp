#pragma once

#include <Crisp/Camera/Camera.hpp>

#include <Crisp/Core/Mouse.hpp>
#include <Crisp/Core/Window.hpp>

namespace crisp
{
class TargetCameraController
{
public:
    TargetCameraController(Window* window);
    ~TargetCameraController();

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

private:
    Window* m_window;
    Camera m_camera;

    glm::vec3 m_target;
    float m_distance;

    float m_panSpeed;
    float m_angularSpeed;

    float m_yaw;
    float m_pitch;

    bool m_isDragging;
    glm::vec2 m_prevMousePos;
};
} // namespace crisp