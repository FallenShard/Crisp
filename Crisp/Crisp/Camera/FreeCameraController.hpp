#pragma once

#include <Crisp/Camera/Camera.hpp>

#include <Crisp/Core/Mouse.hpp>
#include <Crisp/Core/Window.hpp>

namespace crisp
{
    class FreeCameraController
    {
    public:
        FreeCameraController(Window* window);
        FreeCameraController(int32_t viewportWidth, int32_t viewportHeight);
        ~FreeCameraController();

        void setSpeed(float speed);

        void move(float dx, float dy);
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

        float m_speed;
        float m_angularSpeed;

        float m_yaw;
        float m_pitch;

        bool m_isDragging;
        glm::vec2 m_prevMousePos;
    };
}