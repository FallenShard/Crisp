#pragma once

#include <functional>

#include <CrispCore/Event.hpp>

#include "Control.hpp"
#include "Label.hpp"
#include "Panel.hpp"
#include "Animation/PropertyAnimation.hpp"

namespace crisp::gui
{
    class DoubleSlider : public Control
    {
    public:
        DoubleSlider(Form* parentForm);
        virtual ~DoubleSlider();

        void setMinValue(double minValue);
        void setMaxValue(double maxValue);
        void setValue(double value);
        void setPrecision(int precision);
        void setIncrement(double increment);

        virtual void onMouseEntered(float x, float y) override;
        virtual void onMouseExited(float x, float y) override;
        virtual void onMousePressed(float x, float y) override;
        virtual void onMouseMoved(float x, float y) override;
        virtual void onMouseReleased(float x, float y) override;

        virtual void validate() override;

        virtual void draw(const RenderSystem& visitor) const override;

        Event<double> valueChanged;

    private:
        enum class State
        {
            Idle,
            Hover,
            Pressed
        };

        void setState(State state);
        void moveIndicators(double value);
        double getValueFromMousePosition(float x, float y);

        State m_state;

        glm::vec3 m_hoverColor;
        glm::vec3 m_idleColor;
        glm::vec3 m_pressedColor;

        std::shared_ptr<PropertyAnimation<glm::vec4>> m_colorAnim;

        std::unique_ptr<Label> m_label;
        std::unique_ptr<Panel> m_backgroundRect;
        std::unique_ptr<Panel> m_foregroundRect;
        std::unique_ptr<Panel> m_indicatorRect;

        double m_minValue;
        double m_maxValue;
        double m_value;
        double m_increment;
        int m_precision;
    };
}