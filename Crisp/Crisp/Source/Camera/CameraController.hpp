#pragma once

#include <memory>
#include <list>

#include <GLFW/glfw3.h>

#include "FreeCamera.hpp"
#include "TargetCamera.hpp"

namespace crisp
{
    class Animator;

    class CameraController
    {
    public:
        CameraController(GLFWwindow* window);
        ~CameraController();

        void update(float dt);

        void onMousePressed(int button, int mods, double xPos, double yPos);
        void onMouseReleased(int button, int mods, double xPos, double yPos);
        void onMouseMoved(double xPos, double yPos);
        void onMouseWheelScrolled(double offset);

        void resize(int width, int height);

        const AbstractCamera& getCamera() const;

    private:
        void applyRotation(float dt);
        void applyZoom(float dt);

        glm::vec2 filterMouseMoves();

        std::unique_ptr<Animator> m_animator;


        enum class CameraInputState
        {
            Zoom,
            Rotation
        };

        FreeCamera m_camera;
        //TargetCamera m_camera;

        GLFWwindow* m_window;
        CameraInputState m_cameraInputState;
        bool m_useMouseFiltering;

        float m_deltaX;
        float m_deltaY;

        float m_prevX;
        float m_prevY;

        glm::vec2 m_rotationValues;

        bool m_isMoving;

        glm::vec2 m_mousePos;
        glm::vec2 m_prevMousePos;
        glm::vec2 m_screenSize;

        float m_moveSpeed;

        bool m_refreshDeltasWithUpdate;
        std::list<glm::vec2> m_mouseDeltas;
    };
    

}
