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
            Label(RenderSystem* renderSystem, const std::string& text = "Example Text");
            ~Label();

            virtual float getWidth() const override;
            virtual float getHeight() const override;

            void setText(const std::string& text);
            glm::vec2 getTextExtent() const;

            void setColor(ColorPalette color);
            ColorPalette getColor() const;

            virtual void validate() override;

            virtual void draw(RenderSystem& visitor) override;

            unsigned int getTextResourceId() const;

        private:
            unsigned int m_fontId;
            std::string m_text;
            glm::vec2 m_textExtent;

            ColorPalette m_color;

            unsigned int m_textResourceId;
        };
    }
}