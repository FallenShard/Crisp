#include "CameraController.hpp"

#include <iostream>

#include "Animation/Animator.hpp"
#include "Animation/PropertyAnimation.hpp"

#include "Core/InputDispatcher.hpp"

namespace crisp
{
    namespace
    {
        std::ostream& operator<<(std::ostream& out, const glm::vec2& v)
        {
            out << "[" << v.x << ", " << v.y << "]\n";
            return out;
        }
    }

    CameraController::CameraController(InputDispatcher* inputDispatcher)
        : m_window(inputDispatcher->getWindow())
        , m_useMouseFiltering(false)
        , m_isMoving(false)
        , m_moveSpeed(2.0f)
        , m_refreshDeltasOnUpdate(true)
    {
        int width, height;
        glfwGetWindowSize(m_window, &width, &height);
        m_screenSize.x = static_cast<float>(width);
        m_screenSize.y = static_cast<float>(height);

        float aspectRatio = m_screenSize.x / m_screenSize.y;
        m_camera.setupProjection(35.0f, aspectRatio);

        double x, y;
        glfwGetCursorPos(m_window, &x, &y);
        m_prevMousePos.x   = static_cast<float>(x);
        m_prevMousePos.y   = static_cast<float>(y);

        m_animator = std::make_unique<Animator>();

        for (int i = 0; i < MouseFilterListSize; i++)
            m_mouseDeltas.push_front(glm::vec2(0.0f, 0.0f));

        inputDispatcher->mouseButtonPressed.subscribe<CameraController, &CameraController::onMousePressed>(this);
        inputDispatcher->mouseButtonReleased.subscribe<CameraController, &CameraController::onMouseReleased>(this);
        inputDispatcher->mouseMoved.subscribe<CameraController, &CameraController::onMouseMoved>(this);
        inputDispatcher->mouseWheelScrolled.subscribe<CameraController, &CameraController::onMouseWheelScrolled>(this);
    }

    CameraController::~CameraController()
    {
    }

    bool CameraController::update(float dt)
    {
        checkKeyboardInput(dt);

        if (m_refreshDeltasOnUpdate)
        {
            m_mouseDeltas.pop_back();
            m_mouseDeltas.push_front(glm::vec2(0.0f, 0.0f));
        }
        else
        {
            m_refreshDeltasOnUpdate = true;
        }
        
        m_animator->update(dt);

        bool viewChanged = m_camera.update(dt);

        if (viewChanged)
        {
            m_cameraParameters.P = m_camera.getProjectionMatrix();
            m_cameraParameters.V = m_camera.getViewMatrix();
            m_cameraParameters.nearFar = { 0.5f, 100.0f };
            m_cameraParameters.screenSize = m_screenSize;
        }

        return viewChanged;
    }

    void CameraController::onMousePressed(int button, int mods, double xPos, double yPos)
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            m_isMoving = true;

            m_prevMousePos.x = static_cast<float>(xPos);
            m_prevMousePos.y = static_cast<float>(yPos);

            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    void CameraController::onMouseReleased(int button, int mods, double xPos, double yPos)
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            m_isMoving = false;

            m_prevMousePos.x = static_cast<float>(xPos);
            m_prevMousePos.y = static_cast<float>(yPos);

            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    void CameraController::onMouseMoved(double xPos, double yPos)
    {
        auto mousePos = glm::vec2(static_cast<float>(xPos), static_cast<float>(yPos));

        m_mouseDeltas.pop_back();
        m_mouseDeltas.push_front(mousePos - m_prevMousePos);
        m_refreshDeltasOnUpdate = false;

        if (m_isMoving)
        {
            auto delta = m_useMouseFiltering ? filterMouseMoves() : mousePos - m_prevMousePos;
            auto moveAmount = -m_moveSpeed * delta / m_screenSize;
            m_camera.rotate(moveAmount.x, moveAmount.y);
        }

        m_prevMousePos = mousePos;
    }

    void CameraController::onMouseWheelScrolled(double offset)
    {
        //auto zoomAnim = std::make_shared<PropertyAnimation<float>>(0.3, m_camera.getDistance(), static_cast<float>(m_camera.getDistance() - offset), 0.0, Easing::CubicOut);
        //zoomAnim->setUpdater([this](const float& t)
        //{
        //    m_camera.setZoom(t);
        //});
        //m_animator->add(zoomAnim);
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

    const CameraParameters* CameraController::getCameraParameters() const
    {
        return &m_cameraParameters;
    }

    const glm::mat4& CameraController::getViewMatrix() const
    {
        return m_camera.getViewMatrix();
    }

    const glm::mat4& CameraController::getProjectionMatrix() const
    {
        return m_camera.getProjectionMatrix();
    }

    void CameraController::checkKeyboardInput(float dt)
    {
        //if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
        //    m_camera.pan(-3.0f * dt, 0.0f);
        //
        //if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
        //    m_camera.pan(+3.0f * dt, 0.0f);
        if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
            m_camera.strafe(-3.0f * dt);
        
        if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
            m_camera.strafe(+3.0f * dt);

        if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)
            m_camera.walk(-3.0f * dt);

        if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)
            m_camera.walk(3.0f * dt);
    }

    glm::vec2 CameraController::filterMouseMoves()
    {
        glm::vec2 result(0.0f, 0.0f);
        float weightSum = 0.0f;
        float currentWeight = MouseFilterWeight;

        for (auto& delta : m_mouseDeltas)
        {
            result    += currentWeight * delta;
            weightSum += currentWeight;

            currentWeight *= MouseFilterWeightDecay;
        }

        return result / weightSum;
    }
}