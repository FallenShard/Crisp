#pragma once

#include <memory>
#include <list>

#include "FreeCamera.hpp"
#include "TargetCamera.hpp"

#include "Core/Mouse.hpp"

namespace crisp
{
    class Animator;
    class Window;
    class InputDispatcher;

    struct CameraParameters
    {
        glm::mat4 V;
        glm::mat4 P;
        glm::vec2 screenSize;
        glm::vec2 nearFar;
    };

    class CameraController
    {
    public:
        CameraController(Window* window);
        ~CameraController();

        bool update(float dt);

        void onMousePressed(const MouseEventArgs& mouseEventArgs);
        void onMouseReleased(const MouseEventArgs& mouseEventArgs);
        void onMouseMoved(double xPos, double yPos);
        void onMouseWheelScrolled(double offset);

        void resize(int width, int height);

        AbstractCamera& getCamera();
        const AbstractCamera& getCamera() const;
        const CameraParameters* getCameraParameters() const;

        const glm::mat4& getViewMatrix() const;
        const glm::mat4& getProjectionMatrix() const;

        void updateFov(float fovDelta);

    private:
        void checkKeyboardInput(float dt);
        glm::vec2 filterMouseMoves();

        Window* m_window;
        glm::vec2 m_screenSize;

        std::unique_ptr<Animator> m_animator;

        CameraParameters m_cameraParameters;

        FreeCamera m_camera;

        float m_deltaX;
        float m_deltaY;

        glm::vec2 m_rotationValues;

        bool m_isMoving;
        glm::vec2 m_prevMousePos;

        float m_moveSpeed;
        float m_lookSpeed;

        static constexpr size_t MouseFilterListSize    = 10;
        static constexpr float  MouseFilterWeight      = 1.0f;
        static constexpr float  MouseFilterWeightDecay = 0.2f;

        bool m_useMouseFiltering;
        bool m_refreshDeltasOnUpdate;
        std::list<glm::vec2> m_mouseDeltas;
    };
}
