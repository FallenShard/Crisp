#include "CameraController.hpp"

#include <iostream>

#include "Animation/Animator.hpp"
#include "Animation/PropertyAnimation.hpp"

#include "Core/InputDispatcher.hpp"

namespace crisp
{
    CameraController::CameraController(InputDispatcher* inputDispatcher)
        : m_window(inputDispatcher->getWindow())
        , m_useMouseFiltering(true)
        , m_isMoving(false)
        , m_moveSpeed(2.0f)
        , m_refreshDeltasWithUpdate(true)
        , m_targetCamera(RotationStrategy::EulerAngles)
        //, m_cameraState(CameraState::Target)
    {
        int width, height;
        glfwGetWindowSize(m_window, &width, &height);
        m_screenSize.x = static_cast<float>(width);
        m_screenSize.y = static_cast<float>(height);

        float aspectRatio = m_screenSize.x / m_screenSize.y;
        m_targetCamera.setupProjection(35.0f, aspectRatio);
        m_targetCamera.setPosition({ 0.0f, 0.0f, 5.0f });

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

        bool viewChanged = m_targetCamera.update(dt);

        if (viewChanged)
        {
            m_cameraParameters.P = m_targetCamera.getProjectionMatrix();
            m_cameraParameters.V = m_targetCamera.getViewMatrix();
            m_cameraParameters.nearFar = { 0.5f, 100.0f };
            m_cameraParameters.screenSize = m_screenSize;
        }

        return viewChanged;
    }

    void CameraController::onMousePressed(int button, int mods, double xPos, double yPos)
    {
        if (button == GLFW_MOUSE_BUTTON_2)
        {
            m_isMoving = true;

            m_prevMousePos.x = static_cast<float>(xPos);
            m_prevMousePos.y = static_cast<float>(yPos);
        }
    }

    void CameraController::onMouseReleased(int button, int mods, double xPos, double yPos)
    {
        if (button == GLFW_MOUSE_BUTTON_2)
        {
            m_isMoving = false;

            m_prevMousePos.x = static_cast<float>(xPos);
            m_prevMousePos.y = static_cast<float>(yPos);
        }
    }

    void CameraController::onMouseMoved(double xPos, double yPos)
    {
        auto mousePos = glm::vec2(static_cast<float>(xPos), static_cast<float>(yPos));

        m_mouseDeltas.pop_back();
        m_mouseDeltas.push_front(mousePos - m_prevMousePos);
        m_refreshDeltasWithUpdate = false;

        if (m_isMoving)
        {
            auto delta = m_useMouseFiltering ? filterMouseMoves() : mousePos - m_prevMousePos;

            m_targetCamera.move(m_moveSpeed * -delta / m_screenSize);
        }

        m_prevMousePos = mousePos;
    }

    void CameraController::onMouseWheelScrolled(double offset)
    {
        auto zoomAnim = std::make_shared<PropertyAnimation<float>>(0.3, m_targetCamera.getDistance(), m_targetCamera.getDistance() - offset, 0.0, Easing::CubicOut);
        zoomAnim->setUpdater([this](const float& t)
        {
            m_targetCamera.setZoom(t);
        });
        m_animator->add(zoomAnim);
    }

    void CameraController::resize(int width, int height)
    {
        m_screenSize.x = static_cast<float>(width);
        m_screenSize.y = static_cast<float>(height);
        m_targetCamera.setApectRatio(m_screenSize.x / m_screenSize.y);
    }

    const AbstractCamera& CameraController::getCamera() const
    {
        return m_targetCamera;
    }

    const CameraParameters* CameraController::getCameraParameters() const
    {
        return &m_cameraParameters;
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