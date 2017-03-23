#pragma once

#include <memory>
#include <list>

#include <GLFW/glfw3.h>

#include "FreeCamera.hpp"
#include "TargetCamera.hpp"

namespace crisp
{
    class Animator;
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
        CameraController(InputDispatcher* inputDispatcher);
        ~CameraController();

        void update(float dt);

        void onMousePressed(int button, int mods, double xPos, double yPos);
        void onMouseReleased(int button, int mods, double xPos, double yPos);
        void onMouseMoved(double xPos, double yPos);
        void onMouseWheelScrolled(double offset);

        void resize(int width, int height);

        const AbstractCamera& getCamera() const;
        const CameraParameters* getCameraParameters() const;

    private:
        glm::vec2 filterMouseMoves();

        GLFWwindow* m_window;
        glm::vec2 m_screenSize;

        std::unique_ptr<Animator> m_animator;

        CameraParameters m_cameraParameters;

        //enum class CameraState
        //{
        //    Free,
        //    Target
        //};
        //
        //CameraState m_cameraState;
        //FreeCamera m_freeCamera;
        TargetCamera m_targetCamera;
        

        float m_deltaX;
        float m_deltaY;

        glm::vec2 m_rotationValues;

        bool m_isMoving;

        glm::vec2 m_prevMousePos;

        float m_moveSpeed;

        static constexpr size_t MouseFilterListSize    = 10;
        static constexpr float  MouseFilterWeight      = 1.0f;
        static constexpr float  MouseFilterWeightDecay = 0.2f;

        bool m_useMouseFiltering;
        bool m_refreshDeltasWithUpdate;
        std::list<glm::vec2> m_mouseDeltas;
    };
}
