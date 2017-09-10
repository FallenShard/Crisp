#include "Button.hpp"

#include <iostream>

#include "Form.hpp"
#include "Label.hpp"

namespace crisp::gui
{
    Button::Button(Form* parentForm)
        : Control(parentForm)
        , m_state(State::Idle)
        , m_label(std::make_unique<Label>(parentForm, "Button"))
        , m_hoverColor(0.4f, 0.4f, 0.4f)
        , m_idleColor(0.3f, 0.3f, 0.3f)
        , m_pressedColor(0.2f, 0.2f, 0.2f)
        , m_disabledColor(0.65f, 0.65f, 0.65f)
    {
        m_transformId = m_renderSystem->registerTransformResource();
        m_M           = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_sizeHint, 1.0f));

        m_color = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
        m_colorId = m_renderSystem->registerColorResource();
        m_renderSystem->updateColorResource(m_colorId, m_color);

        glm::vec4 color = glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity);
        m_colorAnim = std::make_shared<PropertyAnimation<glm::vec4>>(0.5, color, color, [this](const glm::vec4& t)
        {
            setColor(t);
        }, 0, Easing::SlowOut);
            
        m_label->setParent(this);
        m_label->setAnchor(Anchor::Center);
    }

    Button::~Button()
    {
        m_form->getAnimator()->remove(m_colorAnim);
        m_renderSystem->unregisterTransformResource(m_transformId);
        m_renderSystem->unregisterColorResource(m_colorId);
    }

    void Button::setText(const std::string& text)
    {
        m_label->setText(text);
    }

    void Button::setFontSize(unsigned int fontSize)
    {
        m_label->setFontSize(fontSize);
    }

    void Button::setEnabled(bool enabled)
    {
        if (enabled)
            setState(State::Disabled);
        else
            setState(State::Idle);
    }

    void Button::setIdleColor(const glm::vec3& color)
    {
        m_idleColor = color;
        setValidationFlags(Validation::Color);
    }

    void Button::setPressedColor(const glm::vec3& color)
    {
        m_pressedColor = color;
        setValidationFlags(Validation::Color);
    }

    void Button::setHoverColor(const glm::vec3& color)
    {
        m_hoverColor = color;
        setValidationFlags(Validation::Color);
    }

    void Button::onMouseEntered()
    {
        if (m_state == State::Disabled || m_state != State::Idle)
            return;

        setState(State::Hover);
    }

    void Button::onMouseExited()
    {
        if (m_state == State::Disabled || m_state != State::Hover)
            return;

        setState(State::Idle);
    }

    void Button::onMousePressed(float x, float y)
    {
        if (m_state == State::Disabled)
            return;

        setState(State::Pressed);
    }

    void Button::onMouseReleased(float x, float y)
    {
        if (m_state == State::Disabled)
            return;

        if (getAbsoluteBounds().contains(x, y) && m_state == State::Pressed)
        {
            clicked();

            setState(State::Hover);
        }
        else
        {
            setState(State::Idle);
        }
    }

    void Button::validate()
    {
        if (m_validationFlags & Validation::Geometry)
        {
            auto absPos   = getAbsolutePosition();
            auto absDepth = getAbsoluteDepth();
            auto absSize  = getSize();

            m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(absSize, 1.0f));
            m_renderSystem->updateTransformResource(m_transformId, m_M);
        }

        if (m_validationFlags & Validation::Color)
        {
            m_color.a = getParentAbsoluteOpacity() * m_opacity;
            m_renderSystem->updateColorResource(m_colorId, m_color);
        }
            
        m_label->setValidationFlags(m_validationFlags);
        m_label->validate();
        m_label->clearValidationFlags();
    }

    void Button::draw(RenderSystem& renderSystem)
    {
        renderSystem.drawQuad(m_transformId, m_colorId, m_M[3][2]);
        m_label->draw(renderSystem);
    }

    void Button::setState(State state)
    {
        if (m_state == state)
            return;

        m_state = state;

        glm::vec4 targetColor;

        if (m_state == State::Idle)
        {
            targetColor = glm::vec4(m_idleColor, m_opacity);
        }
        else if (m_state == State::Hover)
        {
            targetColor = glm::vec4(m_hoverColor, m_opacity);
        }
        else if (m_state == State::Pressed)
        {
            targetColor = glm::vec4(m_pressedColor, m_opacity);
        }
        else
        {
            targetColor = glm::vec4(m_disabledColor, m_opacity);
        }

        m_colorAnim->reset(glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity), targetColor);

        if (!m_colorAnim->isActive())
        {
            m_form->getAnimator()->add(m_colorAnim);
        }
    }
}