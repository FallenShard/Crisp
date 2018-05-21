#pragma once

#include <CrispCore/Event.hpp>

#include "Animation/PropertyAnimation.hpp"

#include "Control.hpp"
#include "DrawComponents/ColorRectDrawComponent.hpp"

namespace crisp::gui
{
    class Label;

    class Button : public Control
    {
    public:
        Button(Form* parentForm, std::string = "Button");
        virtual ~Button();

        const std::string& getText() const;
        void setText(const std::string& text);
        void setTextColor(const glm::vec3& color);
        void setTextColor(const glm::vec3& color, State state);
        void setFontSize(unsigned int fontSize);

        void setEnabled(bool enabled);
        void setBackgroundColor(const glm::vec3& color);
        void setBackgroundColor(const glm::vec3& color, State state);

        virtual void onMouseEntered(float x, float y) override;
        virtual void onMouseExited(float x, float y) override;
        virtual void onMousePressed(float x, float y) override;
        virtual void onMouseReleased(float x, float y) override;

        virtual void validate() override;

        virtual void draw(const RenderSystem& renderSystem) const override;

        Event<> clicked;

    private:
        void setState(State state);

        State m_state;

        struct Color
        {
            glm::vec3 background;
            glm::vec3 text;
            glm::vec3 border;
        };

        std::vector<Color> m_stateColors;

        glm::vec4 m_borderColor;
        float m_borderOpacity;

        std::shared_ptr<PropertyAnimation<glm::vec4, Easing::Linear>> m_colorAnim;

        std::shared_ptr<PropertyAnimation<glm::vec4, Easing::Linear>> m_labelColorAnim;
        std::unique_ptr<Label> m_label;

        std::shared_ptr<PropertyAnimation<glm::vec4, Easing::Linear>> m_borderColorAnim;

        ColorRectDrawComponent m_drawComponent;
    };
}