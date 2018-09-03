#include "CameraController.hpp"

#include <iostream>

#include "Animation/Animator.hpp"
#include "Animation/PropertyAnimation.hpp"

#include "Core/InputDispatcher.hpp"
#include "Core/Window.hpp"

namespace crisp
{
    namespace
    {
        std::ostream& operator<<(std::ostream& out, const glm::vec2& v)
        {
            out << "[" << v.x << ", " << v.y << "]";
            return out;
        }
    }

    CameraController::CameraController(Window* window)
        : m_window(window)
        , m_inputDispatcher(window->getInputDispatcher())
        , m_useMouseFiltering(true)
        , m_isMoving(false)
        , m_moveSpeed(2.0f)
        , m_refreshDeltasOnUpdate(true)
    {
        m_screenSize = m_window->getSize();
        float aspectRatio = m_screenSize.x / m_screenSize.y;
        m_camera.setupProjection(35.0f, aspectRatio);

        m_prevMousePos = m_window->getCursorPosition();

        m_animator = std::make_unique<Animator>();

        for (int i = 0; i < MouseFilterListSize; i++)
            m_mouseDeltas.push_front(glm::vec2(0.0f, 0.0f));

        m_inputDispatcher->mouseButtonPressed.subscribe<&CameraController::onMousePressed>(this);
        m_inputDispatcher->mouseButtonReleased.subscribe<&CameraController::onMouseReleased>(this);
        m_inputDispatcher->mouseMoved.subscribe<&CameraController::onMouseMoved>(this);
        m_inputDispatcher->mouseWheelScrolled.subscribe<&CameraController::onMouseWheelScrolled>(this);
    }

    CameraController::~CameraController()
    {
        m_inputDispatcher->mouseButtonPressed.unsubscribe<&CameraController::onMousePressed>(this);
        m_inputDispatcher->mouseButtonReleased.unsubscribe<&CameraController::onMouseReleased>(this);
        m_inputDispatcher->mouseMoved.unsubscribe<&CameraController::onMouseMoved>(this);
        m_inputDispatcher->mouseWheelScrolled.unsubscribe<&CameraController::onMouseWheelScrolled>(this);
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

    void CameraController::onMousePressed(const MouseEventArgs& mouseEventArgs)
    {
        if (mouseEventArgs.button == MouseButton::Right)
        {
            m_isMoving = true;

            m_prevMousePos.x = static_cast<float>(mouseEventArgs.x);
            m_prevMousePos.y = static_cast<float>(mouseEventArgs.y);

            m_window->setCursorState(CursorState::Disabled);
        }
    }

    void CameraController::onMouseReleased(const MouseEventArgs& mouseEventArgs)
    {
        if (mouseEventArgs.button == MouseButton::Right)
        {
            m_isMoving = false;

            m_window->setCursorState(CursorState::Normal);

            m_prevMousePos.x = static_cast<float>(mouseEventArgs.x);
            m_prevMousePos.y = static_cast<float>(mouseEventArgs.y);
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
        if (m_inputDispatcher->isKeyDown(Key::A))
            m_camera.strafe(-3.0f * dt);

        if (m_inputDispatcher->isKeyDown(Key::D))
            m_camera.strafe(+3.0f * dt);

        if (m_inputDispatcher->isKeyDown(Key::S))
            m_camera.walk(-3.0f * dt);

        if (m_inputDispatcher->isKeyDown(Key::W))
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