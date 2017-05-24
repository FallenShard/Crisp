#pragma once

#include <memory>
#include <functional>

#include "Control.hpp"

#include "Label.hpp"

namespace crisp
{
    namespace gui
    {
        class CheckBox : public Control
        {
        public:
            CheckBox(Form* parentForm);
            virtual ~CheckBox();

            bool isChecked() const;
            void setCheckCallback(std::function<void(bool)> checkCallback);
            void setText(const std::string& text);

            virtual glm::vec2 getSize() const;
            virtual Rect<float> getAbsoluteBounds() const;

            virtual void onMouseEntered() override;
            virtual void onMouseExited() override;
            virtual void onMousePressed(float x, float y) override;
            virtual void onMouseReleased(float x, float y) override;

            virtual void validate() override;

            //ColorPalette getColor() const;
            virtual void draw(RenderSystem& visitor) override;

            unsigned int getTexCoordResourceId() const;

        private:
            void updateTexCoordResource();

            enum class State
            {
                Idle,
                Hover,
                Pressed
            };

            State m_state;
            bool m_isChecked;

            std::function<void(bool)> m_checkCallback;

            std::unique_ptr<Label> m_label;

            unsigned int m_texCoordResourceId;
        };
    }
}