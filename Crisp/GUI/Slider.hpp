#pragma once

#include <functional>
#include <vector>

#include <CrispCore/Event.hpp>

#include "Control.hpp"
#include "Label.hpp"
#include "Panel.hpp"
#include "Animation/PropertyAnimation.hpp"

namespace crisp::gui
{
    class Slider : public Control
    {
    public:
        Slider(Form* parentForm);
        virtual ~Slider();

        void setMinValue(int minValue);
        void setMaxValue(int maxValue);
        void setValue(int value);
        void setIncrement(int increment);
        void setValues(std::vector<int> values);

        virtual void onMouseEntered(float x, float y) override;
        virtual void onMouseExited(float x, float y) override;
        virtual void onMousePressed(float x, float y) override;
        virtual void onMouseMoved(float x, float y) override;
        virtual void onMouseReleased(float x, float y) override;

        virtual void validate() override;

        virtual void draw(const RenderSystem& visitor) const override;

        Event<int> valueChanged;

    private:
        enum class State
        {
            Idle,
            Hover,
            Pressed
        };

        void setState(State state);
        void moveIndicators(int value);
        int getValueFromMousePosition(float x, float y);

        State m_state;

        glm::vec3 m_hoverColor;
        glm::vec3 m_idleColor;
        glm::vec3 m_pressedColor;

        std::shared_ptr<PropertyAnimation<glm::vec4, Easing::SlowOut>> m_colorAnim;

        std::unique_ptr<Label> m_label;
        std::unique_ptr<Panel> m_backgroundRect;
        std::unique_ptr<Panel> m_foregroundRect;
        std::unique_ptr<Panel> m_indicatorRect;

        int m_minValue;
        int m_maxValue;
        int m_value;
        int m_increment;

        std::vector<int> m_values;
    };
}