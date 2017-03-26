#pragma once

#include <string>

#include "Control.hpp"
#include "IO/FontLoader.hpp"

namespace crisp
{
    namespace gui
    {
        class Label : public Control
        {
        public:
            Label(Form* parentForm, const std::string& text = "Example Text", unsigned int fontSize = 14);
            ~Label();

            virtual float getWidth() const override;
            virtual float getHeight() const override;

            void setFontSize(unsigned int fontSize);
            void setText(const std::string& text);
            glm::vec2 getTextExtent() const;

            virtual void validate() override;

            virtual void draw(RenderSystem& visitor) override;

            unsigned int getTextResourceId() const;

        private:
            unsigned int m_fontId;
            std::string m_text;
            glm::vec2 m_textExtent;

            unsigned int m_textResourceId;
        };
    }
}