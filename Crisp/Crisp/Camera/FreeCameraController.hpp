#pragma once

#include <Crisp/Camera/Camera.hpp>

#include <Crisp/Core/Mouse.hpp>
#include <Crisp/Core/Window.hpp>

namespace crisp {
class FreeCameraController {
public:
    explicit FreeCameraController(Window& window);
    FreeCameraController(int32_t viewportWidth, int32_t viewportHeight);
    ~FreeCameraController();

    void setPosition(float x, float y, float z);
    void setSpeed(float speed);
    void setFovY(float fovYDegrees);

    void move(float dx, float dz);
    void updateOrientation(float dYaw, float dPitch);

    const Camera& getCamera() const;

    bool update(float dt);

    void onMousePressed(const MouseEventArgs& mouseEventArgs);
    void onMouseReleased(const MouseEventArgs& mouseEventArgs);
    void onMouseMoved(double xPos, double yPos);
    void onMouseWheelScrolled(double offset);

    void onViewportResized(int32_t width, int32_t height);

    CameraParameters getCameraParameters() const;
    ExtendedCameraParameters getExtendedCameraParameters() const;

private:
    Window* m_window{nullptr};
    Camera m_camera;

    float m_speed;
    float m_angularSpeed;

    float m_yaw;
    float m_pitch;

    bool m_hasUpdated = false;
    bool m_isDragging;
    glm::vec2 m_prevMousePos;
};
} // namespace crisp