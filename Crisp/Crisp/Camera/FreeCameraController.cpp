#include <Crisp/Camera/FreeCameraController.hpp>

namespace crisp
{
FreeCameraController::FreeCameraController(Window* window)
    : m_window(window)
    , m_camera(m_window->getSize().x, m_window->getSize().y)
    , m_speed(1.5f)
    , m_angularSpeed(glm::radians(90.0f))
    , m_yaw(0.0f)
    , m_pitch(0.0f)
    , m_isDragging(false)
    , m_prevMousePos(0.0f)
{
    m_window->mouseButtonPressed.subscribe<&FreeCameraController::onMousePressed>(this);
    m_window->mouseButtonReleased.subscribe<&FreeCameraController::onMouseReleased>(this);
    m_window->mouseMoved.subscribe<&FreeCameraController::onMouseMoved>(this);
    m_window->mouseWheelScrolled.subscribe<&FreeCameraController::onMouseWheelScrolled>(this);

    m_camera.translate({ 0.0f, 1.0f, 0.0f });
}

FreeCameraController::FreeCameraController(const int32_t viewportWidth, const int32_t viewportHeight)
    : m_window(nullptr)
    , m_camera(viewportWidth, viewportHeight)
    , m_speed(1.5f)
    , m_angularSpeed(glm::radians(90.0f))
    , m_yaw(0.0f)
    , m_pitch(0.0f)
    , m_isDragging(false)
    , m_prevMousePos(0.0f)
{
    m_camera.translate({ 0.0f, 1.0f, 0.0f });
}

FreeCameraController::~FreeCameraController()
{
    if (m_window)
    {
        m_window->mouseButtonPressed.unsubscribe<&FreeCameraController::onMousePressed>(this);
        m_window->mouseButtonReleased.unsubscribe<&FreeCameraController::onMouseReleased>(this);
        m_window->mouseMoved.unsubscribe<&FreeCameraController::onMouseMoved>(this);
        m_window->mouseWheelScrolled.unsubscribe<&FreeCameraController::onMouseWheelScrolled>(this);
    }
}

void FreeCameraController::setSpeed(const float speed)
{
    m_speed = speed;
}

void FreeCameraController::move(const float dx, const float dz)
{
    const glm::vec3 translation = m_camera.getRightDir() * m_speed * dx + m_camera.getLookDir() * m_speed * dz;
    m_camera.translate(translation);
}

void FreeCameraController::updateOrientation(const float dYaw, const float dPitch)
{
    m_yaw += m_angularSpeed * dYaw;
    m_pitch += m_angularSpeed * dPitch;
    const glm::dquat yaw(glm::angleAxis(m_yaw, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::dquat pitch(glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.0f, 0.0f)));
    m_camera.setOrientation(yaw * pitch);
}

const Camera& FreeCameraController::getCamera() const
{
    return m_camera;
}

bool FreeCameraController::update(const float dt)
{
    bool hasMoved = false;
    if (m_window->isKeyDown(Key::A))
    {
        m_camera.translate(-m_camera.getRightDir() * m_speed * dt);
        hasMoved = true;
    }

    if (m_window->isKeyDown(Key::D))
    {
        m_camera.translate(+m_camera.getRightDir() * m_speed * dt);
        hasMoved = true;
    }

    if (m_window->isKeyDown(Key::S))
    {
        m_camera.translate(-m_camera.getLookDir() * m_speed * dt);
        hasMoved = true;
    }

    if (m_window->isKeyDown(Key::W))
    {
        m_camera.translate(+m_camera.getLookDir() * m_speed * dt);
        hasMoved = true;
    }

    return hasMoved;
}

void FreeCameraController::onMousePressed(const MouseEventArgs& mouseEventArgs)
{
    if (mouseEventArgs.button == MouseButton::Right)
    {
        m_isDragging = true;
        m_window->setCursorState(CursorState::Disabled);

        m_prevMousePos.x = static_cast<float>(mouseEventArgs.x);
        m_prevMousePos.y = static_cast<float>(mouseEventArgs.y);
    }
}

void FreeCameraController::onMouseReleased(const MouseEventArgs& mouseEventArgs)
{
    if (mouseEventArgs.button == MouseButton::Right)
    {
        m_isDragging = false;
        m_window->setCursorState(CursorState::Normal);

        m_prevMousePos.x = static_cast<float>(mouseEventArgs.x);
        m_prevMousePos.y = static_cast<float>(mouseEventArgs.y);
    }
}

void FreeCameraController::onMouseMoved(const double xPos, const double yPos)
{
    const glm::vec2 mousePos(static_cast<float>(xPos), static_cast<float>(yPos));

    if (m_isDragging)
    {
        // In [-1, 1] range
        const auto delta = (mousePos - m_prevMousePos) / glm::vec2(m_window->getSize());
        updateOrientation(-delta.x, -delta.y);
    }

    m_prevMousePos = mousePos;
}

void FreeCameraController::onMouseWheelScrolled(const double offset)
{
    m_camera.setVerticalFov(std::clamp(m_camera.getVerticalFov() - static_cast<float>(offset) * 3.0f, 5.0f, 90.0f));
}

void FreeCameraController::onViewportResized(const int32_t width, const int32_t height)
{
    m_camera.setViewportSize(width, height);
}

CameraParameters FreeCameraController::getCameraParameters() const
{
    CameraParameters params{};
    params.V = m_camera.getViewMatrix();
    params.P = m_camera.getProjectionMatrix();
    params.screenSize = glm::vec2(m_window->getSize());
    params.nearFar = m_camera.getViewDepthRange();
    return params;
}
} // namespace crisp
