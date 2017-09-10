#include "Slider.hpp"

#include <iostream>

#include "Form.hpp"

namespace crisp::gui
{
    Slider::Slider(Form* parentForm)
        : Control(parentForm)
        , m_state(State::Idle)
        , m_currLabel(std::make_unique<Label>(parentForm, "50"))
        , m_currRect(std::make_unique<Panel>(parentForm))
        , m_sliderLine(std::make_unique<Panel>(parentForm))
        , m_hoverColor(0.0f, 1.0f, 1.0f)
        , m_idleColor(0.0f, 0.75f, 0.75f)
        , m_pressedColor(0.0f, 0.3f, 0.3f)
        , m_minValue(0)
        , m_value(50)
        , m_maxValue(100)
    {
        setSizeHint({200.0f, 20.0f});
        m_M = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_sizeHint, 1.0f));

        m_sliderLine->setHorizontalSizingPolicy(SizingPolicy::FillParent, 0.875f);
        m_sliderLine->setSizeHint({ 0.0f, 2.0f });
        m_sliderLine->setPosition({ 0, 9 });
        m_sliderLine->setColor({ 0.0f, 1.0f, 1.0f, 1.0f });
        m_sliderLine->setParent(this);

        m_currRect->setSizeHint({7, 20});
        m_currRect->setPosition({100, 0});
        m_currRect->setColor({ 0.0f, 1.0f, 1.0f, 1.0f });
        m_currRect->setParent(this);

        m_currLabel->setAnchor(Anchor::CenterRight);
        m_currLabel->setPosition({ 0.0f, 0.0f });
        m_currLabel->setParent(this);

        glm::vec4 color = glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity);
        m_colorAnim = std::make_shared<PropertyAnimation<glm::vec4>>(0.5, color, color, [this](const glm::vec4& t)
        {
            setColor(t);
            m_sliderLine->setColor(t);
            m_currRect->setColor(t);
        }, 0, Easing::SlowOut);
    }
    
    Slider::~Slider()
    {
        m_form->getAnimator()->remove(m_colorAnim);
    }

    void Slider::setMinValue(int minValue)
    {
        m_minValue = minValue;
        m_currLabel->setValidationFlags(Validation::Geometry);
    }

    void Slider::setMaxValue(int maxValue)
    {
        m_maxValue = maxValue;
        m_currLabel->setValidationFlags(Validation::Geometry);
    }

    void Slider::setValue(int value)
    {
        auto bounds = m_sliderLine->getAbsoluteBounds();
        float localPos = static_cast<float>(value * 175.0f) / static_cast<float>(m_maxValue - m_minValue);

        float indicatorPos = localPos - m_currRect->getSize().x / 2.0f;
        m_currRect->setPosition({ indicatorPos, 0.0f });
        setValidationFlags(Validation::Geometry);

        m_currLabel->setText(std::to_string(value));

        if (m_value != value) {
            m_value = value;
            valueChanged(m_value);
        }
    }

    void Slider::onMouseEntered()
    {
        if (m_state != State::Idle)
            return;

        setState(State::Hover);
    }

    void Slider::onMouseExited() 
    {
        if (m_state != State::Hover)
            return;

        setState(State::Idle);
    }

    void Slider::onMousePressed(float x, float y)
    {
        auto bounds = m_sliderLine->getAbsoluteBounds();
        float indicatorPos = std::max(0.0f, std::min(x - m_M[3][0] - m_currRect->getSize().x / 2.0f, bounds.width));
        m_currRect->setPosition({ indicatorPos, 0.0f });
        setValidationFlags(Validation::Geometry);

        float t = indicatorPos / bounds.width;
        int newValue = m_minValue + static_cast<int>(t * (m_maxValue - m_minValue));

        m_currLabel->setText(std::to_string(newValue));
        if (m_value != newValue) {
            m_value = newValue;
            valueChanged(m_value);
        }

        setState(State::Pressed);
    }

    void Slider::onMouseMoved(float x, float y)
    {
        if (m_state == State::Pressed)
        {
            auto bounds = m_sliderLine->getAbsoluteBounds();
            float indicatorPos = std::max(0.0f, std::min(x - m_M[3][0] - m_currRect->getSize().x / 2.0f, bounds.width));
            m_currRect->setPosition({ indicatorPos, 0.0f });
            setValidationFlags(Validation::Geometry);

            float t = indicatorPos / bounds.width;
            int newValue = m_minValue + static_cast<int>(t * (m_maxValue - m_minValue));

            m_currLabel->setText(std::to_string(newValue));
            if (m_value != newValue) {
                m_value = newValue;
                valueChanged(m_value);
            }
        }
    }

    void Slider::onMouseReleased(float x, float y)
    {
        if (getAbsoluteBounds().contains(x, y) && m_state == State::Pressed)
        {
            setState(State::Hover);
        }
        else
        {
            setState(State::Idle);
        }
    }

    void Slider::validate()
    {
        if (m_validationFlags & Validation::Geometry)
        {
            auto absPos = getAbsolutePosition();
            auto absDepth = getAbsoluteDepth();
            auto absSize = getSize();

            m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(absSize, 1.0f));
        }

        if (m_validationFlags & Validation::Color)
        {
            m_color.a = getParentAbsoluteOpacity() * m_opacity;
        }

        m_sliderLine->setValidationFlags(m_validationFlags);
        m_sliderLine->validate();
        m_sliderLine->clearValidationFlags();

        m_currRect->setValidationFlags(m_validationFlags);
        m_currRect->validate();
        m_currRect->clearValidationFlags();

        m_currLabel->setValidationFlags(m_validationFlags);
        m_currLabel->validate();
        m_currLabel->clearValidationFlags();
    }

    void Slider::draw(RenderSystem& visitor)
    {
        m_sliderLine->draw(visitor);
        m_currRect->draw(visitor);
        m_currLabel->draw(visitor);
    }

    void Slider::setState(State state)
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
        else
        {
            targetColor = glm::vec4(m_pressedColor, m_opacity);
        }

        m_colorAnim->reset(glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity), targetColor);

        if (!m_colorAnim->isActive())
        {
            m_form->getAnimator()->add(m_colorAnim);
        }
    }
}