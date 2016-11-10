#pragma once

#include <string>

#include <glad/glad.h>

#include "Control.hpp"
#include "FontLoader.hpp"

namespace crisp
{
    namespace gui
    {
        class Label : public Control
        {
        public:
            Label(const Font& font, const std::string& text = "TestLabel");
            ~Label();

            const Font& getFont() const;

            void setText(const std::string& text);
            glm::vec2 getTextExtent() const;

            void setColor(const glm::vec4& color);
            glm::vec4 getColor() const;

            virtual void validate() override;

            virtual void draw(const DrawingVisitor& visitor) const override;
            void drawGeometry() const;

        private:
            std::string m_text;
            glm::vec2 m_textExtent;
            glm::vec4 m_color;

            const Font* m_font;

            GLuint m_vao;
            GLuint m_vbo;
            GLuint m_ibo;
        };
    }
}