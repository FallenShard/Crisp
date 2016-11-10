#pragma once

#include <memory>
#include <functional>

#include <glad/glad.h>

#include "Control.hpp"

#include "Label.hpp"

namespace crisp
{
    namespace gui
    {
        class CheckBox : public Control
        {
        public:
            CheckBox(const Font& font);
            virtual ~CheckBox();

            bool isChecked() const;

            virtual glm::vec2 getSize() const;

            void setText(const std::string& text);
            void setColor(const glm::vec4& color);
            glm::vec4 getColor() const;
            glm::vec4 getRenderedColor() const;

            void setCheckCallback(std::function<void(bool)> checkCallback);

            virtual Rect<float> getBounds() const;

            virtual void onMouseEntered() override;
            virtual void onMouseExited() override;
            virtual void onMousePressed(float x, float y) override;
            virtual void onMouseReleased(float x, float y) override;

            virtual void validate() override;

            virtual void draw(const DrawingVisitor& visitor) const override;

        private:
            enum class State
            {
                Idle,
                Hover,
                Pressed
            };

            State m_state;
            bool m_isChecked;

            std::function<void(bool)> m_checkCallback;

            glm::vec4 m_color;

            std::unique_ptr<Label> m_label;
        };
    }
}