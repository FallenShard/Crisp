#include "Label.hpp"

#include <vector>
#include <iostream>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace crisp
{
    namespace gui
    {
        Label::Label(RenderSystem* renderSystem, const std::string& text)
            : m_color(Green)
            , m_text(text)
            , m_textResourceId(-1)
            , m_textChanged(true)
        {
            m_M = glm::translate(glm::vec3(m_position, 0.0f));

            m_renderSystem = renderSystem; 
            
            m_transformId = m_renderSystem->registerTransformResource();
            m_textResourceId = m_renderSystem->registerTextResource();
            m_textExtent = m_renderSystem->queryTextExtent(text);

            setText(text);
            setPosition(glm::vec2(0, 20));
        }

        Label::~Label()
        {
            m_renderSystem->unregisterTextResource(m_textResourceId);
            m_renderSystem->unregisterTransformResource(m_transformId);
        }

        void Label::setText(const std::string& text)
        {
            m_text = text;

            m_textChanged = true;
            invalidate();
        }

        glm::vec2 Label::getTextExtent() const
        {
            return m_textExtent;
        }

        void Label::setColor(ColorPalette color)
        {
            m_color = color;
        }

        ColorPalette Label::getColor() const
        {
            return m_color;
        }

        void Label::validate()
        {
            if (m_textChanged)
            {
                m_textExtent = m_renderSystem->updateTextResource(m_textResourceId, m_text);
                m_textChanged = false;
            }

            m_M = glm::translate(glm::vec3(m_absolutePosition, m_depth));

            m_renderSystem->updateTransformResource(m_transformId, m_M);
        }

        void Label::draw(RenderSystem& visitor)
        {
            visitor.draw(*this);
        }

        unsigned int Label::getTextResourceId() const
        {
            return m_textResourceId;
        }
    }
}