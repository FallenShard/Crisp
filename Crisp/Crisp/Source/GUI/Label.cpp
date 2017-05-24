#include "Label.hpp"

#include <vector>
#include <iostream>

namespace crisp
{
    namespace gui
    {
        Label::Label(Form* parentForm, const std::string& text, unsigned int fontSize)
            : Control(parentForm)
            , m_text(text)
            , m_textResourceId(-1)
        {
            m_transformId = m_renderSystem->registerTransformResource();
            m_M           = glm::translate(glm::vec3(m_position, m_depthOffset));

            m_color = glm::vec4(1.0f);
            m_colorId = m_renderSystem->registerColorResource();
            m_renderSystem->updateColorResource(m_colorId, m_color);

            m_fontId         = m_renderSystem->getFont("SegoeUI.ttf", fontSize);
            m_textResourceId = m_renderSystem->registerTextResource(m_text, m_fontId);
            m_textExtent     = m_renderSystem->queryTextExtent(m_text, m_fontId);
        }

        Label::~Label()
        {
            m_renderSystem->unregisterTextResource(m_textResourceId);
            m_renderSystem->unregisterTransformResource(m_transformId);
            m_renderSystem->unregisterColorResource(m_colorId);
        }

        float Label::getWidth() const
        {
            return m_textExtent.x;
        }

        float Label::getHeight() const
        {
            return m_textExtent.y;
        }

        Rect<float> Label::getAbsoluteBounds() const
        {
            return{ m_M[3][0], m_M[3][1], m_textExtent.x, m_textExtent.y };
        }

        void Label::setFontSize(unsigned int fontSize)
        {
            m_fontId = m_renderSystem->getFont("SegoeUI.ttf", fontSize);

            m_textExtent = m_renderSystem->updateTextResource(m_textResourceId, m_text, m_fontId);

            setValidationFlags(Validation::Transform);
        }

        void Label::setText(const std::string& text)
        {
            m_text = text;

            m_textExtent = m_renderSystem->updateTextResource(m_textResourceId, m_text, m_fontId);

            setValidationFlags(Validation::Transform);
        }

        glm::vec2 Label::getTextExtent() const
        {
            return m_textExtent;
        }

        void Label::validate()
        {
            if (m_validationFlags & Validation::Transform)
            {
                auto absPos = getAbsolutePosition();
                auto absDepth = getAbsoluteDepth();

                glm::vec2 renderedPos(std::round(absPos.x), std::round(absPos.y + m_textExtent.y));
                m_M = glm::translate(glm::vec3(renderedPos, absDepth));// *glm::scale(glm::vec3(m_scale, m_scale, 1.0f));

                m_renderSystem->updateTransformResource(m_transformId, m_M);
            }

            if (m_validationFlags & Validation::Color)
            {
                m_color.a = getParentAbsoluteOpacity() * m_opacity;
                m_renderSystem->updateColorResource(m_colorId, m_color);
            }
        }

        void Label::draw(RenderSystem& visitor)
        {
            visitor.drawText(m_textResourceId, m_transformId, m_colorId, m_M[3][2]);
        }

        unsigned int Label::getTextResourceId() const
        {
            return m_textResourceId;
        }
    }
}