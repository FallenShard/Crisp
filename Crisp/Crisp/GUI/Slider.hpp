#pragma once

#include <functional>
#include <iomanip>
#include <sstream>
#include <vector>

#include <CrispCore/Event.hpp>

#include "Control.hpp"
#include "Form.hpp"
#include "Label.hpp"
#include "Panel.hpp"
#include <Crisp/Animation/PropertyAnimation.hpp>

namespace crisp::gui
{
template <typename T>
class SliderModel
{
public:
    SliderModel()
        : m_indexValue(50)
        , m_valueCount(101)
        , m_minValue(0)
        , m_maxValue(100)
        , m_increment(1)
        , m_precision(2)
    {
    }

    SliderModel(T minValue, T maxValue)
        : SliderModel()
    {
        setMinValue(minValue);
        setMaxValue(maxValue);
        setValue(minValue + (maxValue - minValue) / T(2));
    }

    void setMinValue(T minValue)
    {
        m_minValue = minValue;
        updateValueCount();
        setIndexValue(calculateIndexValue(m_minValue + m_indexValue * m_increment));
    }

    void setMaxValue(T maxValue)
    {
        m_maxValue = maxValue;
        updateValueCount();
        setIndexValue(calculateIndexValue(m_minValue + m_indexValue * m_increment));
    }

    void setValue(T value)
    {
        setIndexValue(calculateIndexValue(value));
    }

    void setIndexValue(int indexValue)
    {
        if (m_indexValue == indexValue)
            return;

        m_indexValue = indexValue;
        changed(getNormalizedValue(), getValue());
    }

    void setIncrement(T increment)
    {
        // Get the value that corresponds to the current index value
        T value = getValue();

        // Modify the increment and the value count
        m_increment = increment;
        updateValueCount();

        // Recalculate the index value using the original value
        setValue(value);
    }

    void setPrecision(int precision)
    {
        m_precision = precision;
    }

    void setDiscreteValues(std::vector<T> discreteValues)
    {
        m_discreteValues = std::move(discreteValues);
        setIndexValue(static_cast<int>(m_discreteValues.size()) / 2);
    }

    void updateIndexValue(double indicatorNormPos)
    {
        if (!m_discreteValues.empty())
        {
            double rawValue = indicatorNormPos * (m_discreteValues.size() - 1) + 0.5;
            setIndexValue(static_cast<int>(rawValue));
        }
        else
        {
            double rawValue = indicatorNormPos * (m_maxValue - m_minValue);
            double spillOver = std::fmod(rawValue, m_increment);
            double snappedValue =
                spillOver < m_increment / 2.0 ? rawValue - spillOver : rawValue + m_increment - spillOver;

            int indexValue = static_cast<int>(std::round(snappedValue / m_increment));
            if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
            {
                T range = m_maxValue - m_minValue;
                T lastIncrement = std::fmod(range, m_increment);
                if (lastIncrement > 0 && rawValue > range - static_cast<double>(lastIncrement) / 2.0)
                    indexValue = getValueCount() - 1;
            }
            else
            {
                // Fix for the last value
                T range = m_maxValue - m_minValue;
                T lastIncrement = range % m_increment;
                if (lastIncrement > 0 && rawValue > range - static_cast<double>(lastIncrement) / 2.0)
                    indexValue = getValueCount() - 1;
            }
            setIndexValue(indexValue);
        }
    }

    T getValue() const
    {
        return !m_discreteValues.empty()
                   ? m_discreteValues[m_indexValue]
                   : std::max(m_minValue, std::min(m_maxValue, m_minValue + m_indexValue * m_increment));
    }

    float getNormalizedValue() const
    {
        return !m_discreteValues.empty()
                   ? static_cast<float>(m_indexValue) / (getValueCount() - 1)
                   : std::max(0.0f, std::min(1.0f, static_cast<float>(m_indexValue * m_increment) /
                                                       static_cast<float>(m_maxValue - m_minValue)));
    }

    std::string getValueString() const
    {
        if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
        {
            std::stringstream stream;
            stream << std::fixed << std::setprecision(m_precision) << getValue();
            return stream.str();
        }
        else
            return std::to_string(getValue());
    }

    Event<float, T> changed;

private:
    void updateValueCount()
    {
        const T range = m_maxValue - m_minValue;
        m_valueCount = static_cast<int>(range / m_increment) + 1;
        if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
        {
            if (std::fmod(range, m_increment) > 1e-5)
                m_valueCount++;
        }
        else
        {
            if (range % m_increment > 0)
                m_valueCount++;
        }
    }

    int calculateIndexValue(T value) const
    {
        value = std::min(m_maxValue, std::max(m_minValue, value));
        return static_cast<int>((value - m_minValue) / m_increment);
    }

    int getValueCount() const
    {
        return m_discreteValues.empty() ? m_valueCount : static_cast<int>(m_discreteValues.size());
    }

    int m_indexValue;
    int m_valueCount;

    T m_minValue;
    T m_maxValue;
    T m_increment;

    std::vector<T> m_discreteValues;
    int m_precision;
};

template <typename T>
class Slider : public Control
{
public:
    Slider(Form* parentForm)
        : Slider(parentForm, T(0), T(100))
    {
    }

    Slider(Form* parentForm, T minVal, T maxVal)
        : Control(parentForm)
        , m_model(minVal, maxVal)
        , m_state(State::Idle)
        , m_label(std::make_unique<Label>(parentForm, m_model.getValueString()))
        , m_backgroundRect(std::make_unique<Panel>(parentForm))
        , m_foregroundRect(std::make_unique<Panel>(parentForm))
        , m_indicatorRect(std::make_unique<Panel>(parentForm))
        , m_hoverColor(0.0f, 1.0f, 1.0f)
        , m_idleColor(0.0f, 0.75f, 0.75f)
        , m_pressedColor(0.0f, 0.3f, 0.3f)
    {
        setSizeHint({ 200.0f, 20.0f });
        setPadding({ 0.0f, 20.0f }, { 0.0f, 0.0f });
        m_M = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_sizeHint, 1.0f));

        m_backgroundRect->setHorizontalSizingPolicy(SizingPolicy::FillParent);
        m_backgroundRect->setSizeHint({ 0.0f, 2.0f });
        m_backgroundRect->setColor(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
        m_backgroundRect->setAnchor(Anchor::CenterLeft);
        m_backgroundRect->setOrigin(Origin::CenterLeft);
        m_backgroundRect->setParent(this);

        m_foregroundRect->setSizeHint({ m_sizeHint.x * 0.5f, 2.0f });
        m_foregroundRect->setDepthOffset(2.0f);
        m_foregroundRect->setColor(glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
        m_foregroundRect->setAnchor(Anchor::CenterLeft);
        m_foregroundRect->setOrigin(Origin::CenterLeft);
        m_foregroundRect->setParent(this);

        m_indicatorRect->setSizeHint({ 7, 20 });
        m_indicatorRect->setPosition({ m_sizeHint.x * 0.5f, 0.0f });
        m_indicatorRect->setDepthOffset(2.0f);
        m_indicatorRect->setColor(glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
        m_indicatorRect->setAnchor(Anchor::CenterLeft);
        m_indicatorRect->setOrigin(Origin::Center);
        m_indicatorRect->setParent(this);

        m_label->setAnchor(Anchor::CenterRight);
        m_label->setOrigin(Origin::CenterLeft);
        m_label->setPosition({ -10.0f, 0.0f });
        m_label->setParent(this);

        glm::vec4 finalColor = glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity);
        m_colorAnim = std::make_shared<PropertyAnimation<glm::vec4, Easing::SlowOut>>(0.5, finalColor, finalColor,
            [this](const glm::vec4& t)
            {
                setColor(t);
                m_indicatorRect->setColor(t);
                m_foregroundRect->setColor(t);
            });

        m_model.changed.subscribe<&Slider::moveIndicators>(this);
    }

    virtual ~Slider()
    {
        m_form->getAnimator()->remove(m_colorAnim);
    }

    void setMinValue(T minValue)
    {
        m_model.setMinValue(minValue);
    }

    void setMaxValue(T maxValue)
    {
        m_model.setMaxValue(maxValue);
    }

    void setIndexValue(int indexValue)
    {
        m_model.setIndexValue(indexValue);
    }

    void setValue(T value)
    {
        m_model.setValue(value);
    }

    void setIncrement(T increment)
    {
        m_model.setIncrement(increment);
    }

    void setPrecision(int precision)
    {
        m_model.setPrecision(precision);
    }

    void setDiscreteValues(std::vector<T> values)
    {
        m_model.setDiscreteValues(std::move(values));
    }

    virtual void onMouseEntered(float /*x*/, float /*y*/) override
    {
        if (m_state != State::Idle)
            return;

        setState(State::Hover);
    }

    virtual void onMouseExited(float /*x*/, float /*y*/) override
    {
        if (m_state != State::Hover)
            return;

        setState(State::Idle);
    }

    virtual bool onMousePressed(float x, float y) override
    {
        m_model.updateIndexValue(getIndicatorNormPos(x, y));

        setState(State::Pressed);
        m_form->setFocusedControl(this);
        return true;
    }

    virtual void onMouseMoved(float x, float y) override
    {
        if (m_state == State::Pressed)
            m_model.updateIndexValue(getIndicatorNormPos(x, y));
    }

    virtual bool onMouseReleased(float x, float y) override
    {
        if (getAbsoluteBounds().contains(x, y) && m_state == State::Pressed)
        {
            setState(State::Hover);
        }
        else
        {
            setState(State::Idle);
        }

        moveIndicators(m_model.getNormalizedValue(), m_model.getValue());
        setValidationFlags(Validation::Geometry);

        m_form->setFocusedControl(nullptr);

        return true;
    }

    virtual void validate() override
    {
        moveIndicators(m_model.getNormalizedValue(), m_model.getValue());

        auto absPos = getAbsolutePosition();
        auto absDepth = getAbsoluteDepth();
        auto absSize = getSize();
        m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(absSize, 1.0f));

        m_color.a = getParentAbsoluteOpacity() * m_opacity;

        m_backgroundRect->validateAndClearFlags();
        m_foregroundRect->validateAndClearFlags();
        m_indicatorRect->validateAndClearFlags();
        m_label->validateAndClearFlags();
    }

    virtual void draw(const RenderSystem& visitor) const override
    {
        m_backgroundRect->draw(visitor);
        m_foregroundRect->draw(visitor);
        m_indicatorRect->draw(visitor);
        m_label->draw(visitor);

        // visitor.drawDebugRect(getAbsoluteBounds());
    }

    Event<T> valueChanged;

private:
    enum class State
    {
        Idle,
        Hover,
        Pressed
    };

    void setState(State state)
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
            m_form->getAnimator()->add(m_colorAnim);
    }

    void moveIndicators(float normValue, T value)
    {
        Rect<float> bounds = m_backgroundRect->getAbsoluteBounds();
        float localPos = normValue * bounds.width;
        m_indicatorRect->setPosition({ localPos, 0.0f });
        m_foregroundRect->setSizeHint({ localPos, 2.0f });
        setValidationFlags(Validation::Geometry);

        m_label->setText(m_model.getValueString());
        valueChanged(value);
    }

    float getIndicatorNormPos(float x, float /*y*/)
    {
        Rect<float> bounds = m_backgroundRect->getAbsoluteBounds();
        float indicatorPos = std::max(0.0f, std::min(x - m_M[3][0], bounds.width));
        return std::max(0.0f, std::min(1.0f, indicatorPos / bounds.width));
    }

    SliderModel<T> m_model;
    State m_state;

    glm::vec3 m_hoverColor;
    glm::vec3 m_idleColor;
    glm::vec3 m_pressedColor;

    std::shared_ptr<PropertyAnimation<glm::vec4, Easing::SlowOut>> m_colorAnim;

    std::unique_ptr<Label> m_label;
    std::unique_ptr<Panel> m_backgroundRect;
    std::unique_ptr<Panel> m_foregroundRect;
    std::unique_ptr<Panel> m_indicatorRect;
};

using IntSlider = Slider<int>;
using DoubleSlider = Slider<double>;
} // namespace crisp::gui