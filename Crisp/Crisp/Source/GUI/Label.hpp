#pragma once

#include <string>

#include "Control.hpp"
#include "FontLoader.hpp"

namespace crisp
{
    namespace gui
    {
        class Label : public Control
        {
        public:
            Label(RenderSystem* renderSystem, const std::string& text = "TestLabel");
            ~Label();

            void setText(const std::string& text);
            glm::vec2 getTextExtent() const;

            void setColor(ColorPalette color);
            ColorPalette getColor() const;

            virtual void validate() override;

            virtual void draw(RenderSystem& visitor) override;

            unsigned int getTextResourceId() const;

        private:
            std::string m_text;
            glm::vec2 m_textExtent;
            unsigned int m_textResourceId;
            bool m_textChanged;
            
            ColorPalette m_color;
        };
    }
}