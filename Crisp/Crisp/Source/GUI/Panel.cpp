#include "Panel.hpp"

namespace crisp
{
    namespace gui
    {
        Panel::Panel(Form* parentForm)
            : ControlGroup(parentForm)
        {
            m_transformId = m_renderSystem->registerTransformResource();

            m_color = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
            m_colorId = m_renderSystem->registerColorResource();
            m_renderSystem->updateColorResource(m_colorId, m_color);
        }

        Panel::~Panel()
        {
            m_renderSystem->unregisterTransformResource(m_transformId);
            m_renderSystem->unregisterColorResource(m_colorId);
        }

        void Panel::validate()
        {
            if (m_validationFlags & Validation::Color)
            {
                m_color.a = getParentAbsoluteOpacity() * m_opacity;
                m_renderSystem->updateColorResource(m_colorId, m_color);
            }

            ControlGroup::validate();

            if (m_validationFlags & Validation::Transform)
            {
                m_renderSystem->updateTransformResource(m_transformId, m_M);
            }
        }

        void Panel::draw(RenderSystem& visitor)
        {
            visitor.drawQuad(m_transformId, m_colorId, m_M[3][2]);

            ControlGroup::draw(visitor);
        }
        
        void Panel::setClickCallback(std::function<void()> callback)
        {
            m_clickCallback = callback;
        }

        void Panel::onMouseReleased(float x, float y)
        {
            if (m_clickCallback != nullptr && getAbsoluteBounds().contains(x, y))
            {
                m_clickCallback();
            }

            ControlGroup::onMouseReleased(x, y);
        }
    }
}