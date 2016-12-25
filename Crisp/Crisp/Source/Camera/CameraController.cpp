#include "CameraController.hpp"

#include <iostream>

#include "Animation/Animator.hpp"
#include "Animation/PropertyAnimation.hpp"

namespace crisp
{
    namespace
    {
        size_t MouseFilterListSize = 10;
    }

    CameraController::CameraController(GLFWwindow* window)
        : m_window(window)
        , m_cameraInputState(CameraInputState::Rotation)
        , m_useMouseFiltering(true)
        , m_isMoving(false)
        , m_refreshDeltasWithUpdate(true)
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        m_screenSize.x = static_cast<float>(width);
        m_screenSize.y = static_cast<float>(height);
        m_camera.setupProjection(35.0f, m_screenSize.x / m_screenSize.y, 0.5f);

        double x, y;
        glfwGetCursorPos(window, &x, &y);
        m_mousePos.x = static_cast<float>(x);
        m_mousePos.y = static_cast<float>(y);
        m_prevMousePos = m_mousePos;

        m_animator = std::make_unique<Animator>();

        m_moveSpeed = 5.0f;

        for (int i = 0; i < MouseFilterListSize; i++)
            m_mouseDeltas.push_front(glm::vec2(0.0f, 0.0f));
    }

    CameraController::~CameraController()
    {
    }

    void CameraController::update(float dt)
    {
        if (m_refreshDeltasWithUpdate)
        {
            m_mouseDeltas.pop_back();
            m_mouseDeltas.push_front(glm::vec2(0.0f, 0.0f));
        }
        else
        {
            m_refreshDeltasWithUpdate = true;
        }
        
        m_animator->update(dt);
        //if (glfwGetKey(m_window, GLFW_KEY_W))
        //    m_camera.zoom(3.0f * dt);
        //
        //if (glfwGetKey(m_window, GLFW_KEY_S))
        //    m_camera.zoom(3.0f * -dt);
        //
        //if (glfwGetKey(m_window, GLFW_KEY_D))
        //    m_camera.move(dt, dt);
        //
        //if (glfwGetKey(m_window, GLFW_KEY_A))
        //    m_camera.move(-dt, -dt);

        //m_camera.update(dt);
        //
        //if (m_cameraInputState == CameraInputState::Rotation)
        //    applyRotation(dt);
        //else
        //    applyZoom(dt);

        m_camera.update(dt);
    }

    void CameraController::onMousePressed(int button, int mods, double xPos, double yPos)
    {
        if (button == GLFW_MOUSE_BUTTON_2)
        {
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            m_isMoving = true;

            m_mousePos.x = static_cast<float>(xPos);
            m_mousePos.y = static_cast<float>(yPos);
            m_prevMousePos = m_mousePos;
        }
    }

    void CameraController::onMouseReleased(int button, int mods, double xPos, double yPos)
    {
        if (button == GLFW_MOUSE_BUTTON_2)
        {
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_isMoving = false;

            m_mousePos.x = static_cast<float>(xPos);
            m_mousePos.y = static_cast<float>(yPos);
            m_prevMousePos = m_mousePos;
        }
    }

    void CameraController::onMouseMoved(double xPos, double yPos)
    {
        m_prevMousePos = m_mousePos;
        m_mousePos = glm::vec2(static_cast<float>(xPos), static_cast<float>(yPos));

        m_mouseDeltas.pop_back();
        m_mouseDeltas.push_front(m_mousePos - m_prevMousePos);
        m_refreshDeltasWithUpdate = false;

        if (m_isMoving)
        {
            auto delta = m_useMouseFiltering ? filterMouseMoves() : m_mousePos - m_prevMousePos;

            if (glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL))
            {
                m_camera.pan(-delta.x / m_screenSize.x, delta.y / m_screenSize.y);
            }
            else
            {
                m_camera.move(-m_moveSpeed * delta / m_screenSize);
            }
        }
    }

    void CameraController::onMouseWheelScrolled(double offset)
    {
        auto zoomAnim = std::make_shared<PropertyAnimation<float>>(0.3, m_camera.getDistance(), m_camera.getDistance() - offset, 0.0, Easing::CubicOut);
        zoomAnim->setUpdater([this](const float& t)
        {
            m_camera.setZoom(t);
        });
        m_animator->add(zoomAnim);
        //m_camera.zoom(offset);
    }

    void CameraController::resize(int width, int height)
    {
        m_screenSize.x = static_cast<float>(width);
        m_screenSize.y = static_cast<float>(height);
        m_camera.setApectRatio(m_screenSize.x / m_screenSize.y);
    }

    const AbstractCamera& CameraController::getCamera() const
    {
        return m_camera;
    }

    void CameraController::applyZoom(float dt)
    {

    }

    void CameraController::applyRotation(float dt)
    {
        double x, y;
        glfwGetCursorPos(m_window, &x, &y);
        m_deltaX += static_cast<float>(y - m_prevY) / 5.0f;
        m_deltaY += static_cast<float>(m_prevX - x) / 5.0f;
        if (m_useMouseFiltering)
        {
            m_rotationValues = filterMouseMoves();
        }
        else
        {
            m_rotationValues.x = m_deltaX;
            m_rotationValues.y = m_deltaY;
        }
    }

    glm::vec2 CameraController::filterMouseMoves()
    {
        float weight = 1.0f;
        float weightDecay = 0.2f;

        glm::vec2 result(0.0f, 0.0f);
        float weightSum = 0.0f;

        for (auto& delta : m_mouseDeltas)
        {
            result += weight * delta;
            weightSum += weight;

            weight *= weightDecay;
        }

        return result / weightSum;
    }
}