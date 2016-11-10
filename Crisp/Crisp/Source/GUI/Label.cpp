#include "Label.hpp"

#include <vector>
#include <iostream>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace crisp
{
    namespace gui
    {
        Label::Label(const Font& font, const std::string& text)
            : m_font(nullptr)
            , m_color(0, 1, 0, 1)
            , m_text(text)
        {
            m_M = glm::translate(glm::vec3(m_position, 0.0f));
            m_font = &font;

            glCreateBuffers(1, &m_vbo);
            glNamedBufferData(m_vbo, 4 * m_text.size() * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);

            glCreateBuffers(1, &m_ibo);
            glNamedBufferData(m_ibo, 2 * m_text.size() * sizeof(glm::uvec3), nullptr, GL_DYNAMIC_DRAW);

            glCreateVertexArrays(1, &m_vao);
            glEnableVertexArrayAttrib(m_vao, 0);
            glVertexArrayAttribFormat(m_vao, 0, 4, GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(m_vao, 0, 0);

            glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, sizeof(glm::vec4));
            glVertexArrayElementBuffer(m_vao, m_ibo);

            setText(text);
            setPosition(glm::vec2(0, 20));
        }

        Label::~Label()
        {
            glDeleteVertexArrays(1, &m_vao);
            glDeleteBuffers(1, &m_vbo);
            glDeleteBuffers(1, &m_ibo);
        }

        const Font& Label::getFont() const
        {
            return *m_font;
        }

        void Label::setText(const std::string& text)
        {
            m_text = text;

            unsigned short ind = 0;
            float currentX = 0.f;
            float currentY = 0.f;
            float atlasWidth = static_cast<float>(m_font->width);
            float atlasHeight = static_cast<float>(m_font->height);

            std::vector<glm::vec4> verts;
            std::vector<glm::u16vec3> indices;

            m_textExtent = glm::vec2(0.0f, 0.0f);
            for (auto& character : m_text)
            {
                auto& gInfo = m_font->glyphs[character];
                float x1 = currentX + gInfo.bmpLeft;
                float y1 = currentY - gInfo.bmpTop;
                float x2 = x1 + gInfo.bmpWidth;
                float y2 = y1 + gInfo.bmpHeight;

                // Advance the cursor to the start of the next character
                currentX += gInfo.advanceX;
                currentY += gInfo.advanceY;

                verts.emplace_back(glm::vec4{x1, y1, gInfo.atlasOffsetX, 0.0f});
                verts.emplace_back(glm::vec4{x2, y1, gInfo.atlasOffsetX + gInfo.bmpWidth / atlasWidth, 0.0f});
                verts.emplace_back(glm::vec4{x2, y2, gInfo.atlasOffsetX + gInfo.bmpWidth / atlasWidth, gInfo.bmpHeight / atlasHeight});
                verts.emplace_back(glm::vec4{x1, y2, gInfo.atlasOffsetX, gInfo.bmpHeight / atlasHeight});

                indices.emplace_back(glm::u16vec3{ind + 0, ind + 2, ind + 1});
                indices.emplace_back(glm::u16vec3{ind + 0, ind + 3, ind + 2});

                ind += 4;

                
                m_textExtent.y = std::max(m_textExtent.y, gInfo.bmpHeight);
            }

            m_textExtent.x += currentX;

            glNamedBufferData(m_vbo, verts.size() * sizeof(glm::vec4), verts.data(), GL_DYNAMIC_DRAW);
            glNamedBufferData(m_ibo, indices.size() * sizeof(glm::u16vec3), indices.data(), GL_DYNAMIC_DRAW);

            invalidate();
        }

        glm::vec2 Label::getTextExtent() const
        {
            return m_textExtent;
        }

        void Label::setColor(const glm::vec4& color)
        {
            m_color = color;
        }

        glm::vec4 Label::getColor() const
        {
            return m_color;
        }

        void Label::validate()
        {
            m_M = glm::translate(glm::vec3(m_absolutePosition, 0.0f));
        }

        void Label::draw(const DrawingVisitor& visitor) const
        {
            visitor.draw(*this);
        }

        void Label::drawGeometry() const
        {
            glBindVertexArray(m_vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_text.size()) * 6, GL_UNSIGNED_SHORT, nullptr);
            glBindVertexArray(0);
        }
    }
}