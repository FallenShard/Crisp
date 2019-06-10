#include "Slider.hpp"

#include <iostream>

#include "Form.hpp"

namespace crisp::gui
{
    namespace
    {
        static constexpr float BarWidthPercent = 7.0f / 8.0f;
        static constexpr float IndicatorWidth = 7.0f;

        static const glm::vec4 BackgroundColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        static const glm::vec4 ForegroundColor = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);

        static const glm::vec2 IndicatorSize = glm::vec2(7.0f, 20.0f);
    }

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

        m_backgroundRect->setHorizontalSizingPolicy(SizingPolicy::FillParent, BarWidthPercent);
        m_backgroundRect->setSizeHint({ 0.0f, 2.0f });
        m_backgroundRect->setPosition({ 0.0f, 9.0f });
        m_backgroundRect->setColor(BackgroundColor);
        m_backgroundRect->setParent(this);

        m_foregroundRect->setSizeHint({ m_sizeHint.x * BarWidthPercent * 0.5f, 2.0f });
        m_foregroundRect->setPosition({ 0.0f, 9.0f });
        m_foregroundRect->setDepthOffset(2.0f);
        m_foregroundRect->setColor(ForegroundColor);
        m_foregroundRect->setParent(this);

        m_indicatorRect->setSizeHint(IndicatorSize);
        m_indicatorRect->setPosition({ m_sizeHint.x * BarWidthPercent * 0.5f, 0.0f });
        m_indicatorRect->setDepthOffset(2.0f);
        m_indicatorRect->setColor(ForegroundColor);
        m_indicatorRect->setAnchor(Anchor::CenterLeft);
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
        m_value = value;

        int v = m_values.empty() ? m_value : m_values[m_value];

        m_label->setText(std::to_string(v));
        valueChanged(v);
    }

    void Slider::setIncrement(int increment)
    {
        m_increment = increment;
    }

    void Slider::setValues(std::vector<int> values)
    {
        m_values = values;
        setMinValue(0);
        setMaxValue(static_cast<int>(m_values.size()) - 1);
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
        setValue(getValueFromMousePosition(x, y));

        setState(State::Pressed);
        m_form->setFocusedControl(this);
    }

    void Slider::onMouseMoved(float x, float y)
    {
        if (m_state == State::Pressed)
            setValue(getValueFromMousePosition(x, y));
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
        Rect<float> bounds = m_backgroundRect->getAbsoluteBounds();
        float localPos = static_cast<float>((value - m_minValue) * bounds.width) / static_cast<float>(m_maxValue - m_minValue);
        m_indicatorRect->setPosition({ localPos - m_indicatorRect->getSize().x / 2.0f, 0.0f });
        m_foregroundRect->setSizeHint({ localPos, 2.0f });
        setValidationFlags(Validation::Geometry);
    }

    int Slider::getValueFromMousePosition(float x, float y)
    {
        Rect<float> bounds = m_backgroundRect->getAbsoluteBounds();
        float indicatorPos = std::max(0.0f, std::min(x - m_M[3][0], bounds.width));

        float t = std::max(0.0f, std::min(1.0f, indicatorPos / bounds.width));
        float rawValue = t * (m_maxValue - m_minValue);
        float spillOver = std::fmod(rawValue, m_increment);
        float snappedValue = spillOver < m_increment / 2.0 ? rawValue - spillOver : rawValue + m_increment - spillOver;

        return static_cast<int>(std::round(snappedValue + m_minValue));
    }
}