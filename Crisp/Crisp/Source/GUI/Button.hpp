#pragma once

#include <functional>

#include "Control.hpp"
#include "Label.hpp"
#include "Animation/PropertyAnimation.hpp"

namespace crisp
{
    namespace gui
    {
        class Button : public Control
        {
        public:
            Button(Form* parentForm);
            virtual ~Button();

            void setClickCallback(std::function<void()> callback);
            void click();
            void setText(const std::string& text);

            virtual void onMouseEntered() override;
            virtual void onMouseExited() override;
            virtual void onMousePressed(float x, float y) override;
            virtual void onMouseReleased(float x, float y) override;

            virtual void validate() override;

            virtual void draw(RenderSystem& visitor) override;

        private:
            enum class State
            {
                Idle,
                Hover,
                Pressed
            };

            void setState(State state);

            State m_state;

            std::function<void()> m_clickCallback;

            std::shared_ptr<PropertyAnimation<glm::vec4>> m_colorAnim;

            std::unique_ptr<Label> m_label;
        };
    }
}