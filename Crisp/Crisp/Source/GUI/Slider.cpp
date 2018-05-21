#include "Slider.hpp"

#include <iostream>

#include "Form.hpp"

namespace crisp::gui
{
    Slider::Slider(Form* parentForm)
        : Control(parentForm)
        , m_state(State::Idle)
        , m_label(std::make_unique<Label>(parentForm, "50"))
        , m_backgroundRect(std::make_unique<Panel>(parentForm))
        , m_foregroundRect(std::make_unique<Panel>(parentForm))
        , m_indicatorRect(std::make_unique<Panel>(parentForm))
        , m_hoverColor(0.0f, 1.0f, 1.0f)
        , m_idleColor(0.0f, 0.75f, 0.75f)
        , m_pressedColor(0.0f, 0.3f, 0.3f)
        , m_minValue(0)
        , m_value(50)
        , m_maxValue(100)
        , m_increment(1)
    {
        setSizeHint({200.0f, 20.0f});
        m_M = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_sizeHint, 1.0f));

        m_backgroundRect->setHorizontalSizingPolicy(SizingPolicy::FillParent, 0.875f);
        m_backgroundRect->setSizeHint({ 0.0f, 10.0f });
        m_backgroundRect->setPosition({ 0, 5 });
        m_backgroundRect->setColor({ 0.5f, 0.5f, 0.5f, 1.0f });
        m_backgroundRect->setParent(this);

        m_foregroundRect->setSizeHint({ 200.0f * 0.875f * 0.5f, 6.0f });
        m_foregroundRect->setPosition({ 0, 7 });
        m_foregroundRect->setColor({ 0.0f, 1.0f, 1.0f, 1.0f });
        m_foregroundRect->setParent(this);

        m_indicatorRect->setSizeHint({7, 20});
        m_indicatorRect->setPosition({100 * 0.875f, 0});
        m_indicatorRect->setColor({ 0.0f, 1.0f, 1.0f, 1.0f });
        m_indicatorRect->setParent(this);

        m_label->setAnchor(Anchor::CenterRight);
        m_label->setPosition({ 0.0f, 0.0f });
        m_label->setParent(this);

        glm::vec4 color = glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity);
        m_colorAnim = std::make_shared<PropertyAnimation<glm::vec4, Easing::SlowOut>>(0.5, color, color, [this](const glm::vec4& t)
        {
            setColor(t);
            m_indicatorRect->setColor(t);
            m_foregroundRect->setColor(t);
        });
    }

    Slider::~Slider()
    {
        m_form->getAnimator()->remove(m_colorAnim);
    }

    void Slider::setMinValue(int minValue)
    {
        m_minValue = minValue;
        setValue(m_value);
        m_label->setValidationFlags(Validation::Geometry);
    }

    void Slider::setMaxValue(int maxValue)
    {
        m_maxValue = maxValue;
        setValue(m_value);
        m_label->setValidationFlags(Validation::Geometry);
    }

    void Slider::setValue(int value)
    {
        value = std::min(m_maxValue, std::max(m_minValue, value));
        if (m_value == value)
            return;

        moveIndicators(value);
        setValidationFlags(Validation::Geometry);

        m_label->setText(std::to_string(value));

        m_value = value;
        valueChanged(m_value);
    }

    void Slider::setIncrement(int increment)
    {
        m_increment = increment;
    }

    void Slider::setValues(std::vector<int> values)
    {
        m_values = values;
        setMinValue(0);
        setMaxValue(m_values.size() - 1);
        setIncrement(1);
    }

    void Slider::onMouseEntered(float x, float y)
    {
        if (m_state != State::Idle)
            return;

        setState(State::Hover);
    }

    void Slider::onMouseExited(float x, float y)
    {
        if (m_state != State::Hover)
            return;

        setState(State::Idle);
    }

    void Slider::onMousePressed(float x, float y)
    {
        int newValue = getValueFromMousePosition(x, y);

        m_label->setText(std::to_string(newValue));
        if (m_value != newValue) {
            m_value = newValue;
            valueChanged(m_value);
        }

        setState(State::Pressed);
        m_form->setFocusedControl(this);
    }

    void Slider::onMouseMoved(float x, float y)
    {
        if (m_state == State::Pressed)
        {
            int newValue = getValueFromMousePosition(x, y);

            m_label->setText(std::to_string(newValue));
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

        moveIndicators(m_value);
        setValidationFlags(Validation::Geometry);

        m_form->setFocusedControl(nullptr);
    }

    void Slider::validate()
    {
        moveIndicators(m_value);

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

        m_backgroundRect->setValidationFlags(m_validationFlags);
        m_backgroundRect->validate();
        m_backgroundRect->clearValidationFlags();

        m_foregroundRect->setValidationFlags(m_validationFlags);
        m_foregroundRect->validate();
        m_foregroundRect->clearValidationFlags();

        m_indicatorRect->setValidationFlags(m_validationFlags);
        m_indicatorRect->validate();
        m_indicatorRect->clearValidationFlags();

        m_label->setValidationFlags(m_validationFlags);
        m_label->validate();
        m_label->clearValidationFlags();
    }

    void Slider::draw(const RenderSystem& visitor) const
    {
        m_backgroundRect->draw(visitor);
        m_foregroundRect->draw(visitor);
        m_indicatorRect->draw(visitor);
        m_label->draw(visitor);
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

    void Slider::moveIndicators(int value)
    {
        if (!m_values.empty())
        {
            for (int i = 0; i < m_values.size(); i++)
                if (m_values[i] == value)
                {
                    value = i;
                    break;
                }
        }

        auto bounds = m_backgroundRect->getAbsoluteBounds();
        float localPos = static_cast<float>((value - m_minValue) * bounds.width) / static_cast<float>(m_maxValue - m_minValue);
        float indicatorPos = localPos - m_indicatorRect->getSize().x / 2.0f;
        m_indicatorRect->setPosition({ indicatorPos, 0.0f });
        m_foregroundRect->setSizeHint({ localPos, 6.0f });
    }

    int Slider::getValueFromMousePosition(float x, float y)
    {
        auto bounds = m_backgroundRect->getAbsoluteBounds();
        float indicatorPos = std::max(0.0f, std::min(x - m_M[3][0] - m_indicatorRect->getSize().x / 2.0f, bounds.width));
        m_indicatorRect->setPosition({ indicatorPos, 0.0f });
        m_foregroundRect->setSizeHint({ indicatorPos, 6.0f });
        setValidationFlags(Validation::Geometry);

        float t = indicatorPos / bounds.width;
        int valueSpan = m_maxValue - m_minValue;

        int rawValue = static_cast<int>(std::round(m_minValue + t * valueSpan));
        int dd = (rawValue - m_minValue + m_increment / 2) / m_increment;
        int finalValue = std::min(std::max(m_minValue + dd * m_increment, m_minValue), m_maxValue);
        return m_values.empty() ? finalValue : m_values[finalValue];
    }
}