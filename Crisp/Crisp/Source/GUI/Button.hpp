#pragma once

#include <functional>

#include <CrispCore/Event.hpp>

#include "Control.hpp"
#include "Animation/PropertyAnimation.hpp"

namespace crisp::gui
{
    class Label;

    class Button : public Control
    {
    public:
        Button(Form* parentForm);
        virtual ~Button();

        void setText(const std::string& text);
        void setFontSize(unsigned int fontSize);

        void setEnabled(bool enabled);
        void setIdleColor(const glm::vec3& color);
        void setPressedColor(const glm::vec3& color);
        void setHoverColor(const glm::vec3& color);

        virtual void onMouseEntered() override;
        virtual void onMouseExited() override;
        virtual void onMousePressed(float x, float y) override;
        virtual void onMouseReleased(float x, float y) override;

        virtual void validate() override;

        virtual void draw(RenderSystem& renderSystem) override;

        Event<> clicked;

    private:
        enum class State
        {
            Idle,
            Hover,
            Pressed,
            Disabled
        };

        void setState(State state);

        State m_state;

        glm::vec3 m_hoverColor;
        glm::vec3 m_idleColor;
        glm::vec3 m_pressedColor;
        glm::vec3 m_disabledColor;

        std::shared_ptr<PropertyAnimation<glm::vec4>> m_colorAnim;

        std::unique_ptr<Label> m_label;

        unsigned int m_transformId;
        unsigned int m_colorId;
    };
}