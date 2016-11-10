#pragma once

#include <functional>

#include "Control.hpp"
#include "Label.hpp"

namespace crisp
{
    namespace gui
    {
        class Button : public Control
        {
        public:
            Button(const Font& font);
            virtual ~Button();

            void setClickCallback(std::function<void()> callback);

            void setText(const std::string& text);

            virtual void onMouseEntered() override;
            virtual void onMouseExited() override;
            virtual void onMousePressed(float x, float y) override;
            virtual void onMouseReleased(float x, float y) override;

            virtual void validate() override;

            virtual void draw(const DrawingVisitor& visitor) const override;

            glm::vec4 getColorOffset() const;

        private:
            enum class State
            {
                Idle,
                Hover,
                Pressed
            };

            State m_state;

            std::function<void()> m_clickCallback;

            std::unique_ptr<Label> m_label;
        };
    }
}