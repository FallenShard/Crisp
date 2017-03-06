#include "Panel.hpp"

namespace crisp
{
    namespace gui
    {
        Panel::Panel(RenderSystem* renderSystem)
            : m_color(DarkGray)
        {
            m_renderSystem = renderSystem;
            m_transformId = m_renderSystem->registerTransformResource();
        }

        Panel::~Panel()
        {
            m_renderSystem->unregisterTransformResource(m_transformId);
        }

        void Panel::validate()
        {
            ControlGroup::validate();

            m_renderSystem->updateTransformResource(m_transformId, m_M);
        }

        void Panel::setColor(ColorPalette color)
        {
            m_color = color;
        }

        ColorPalette Panel::getColor() const
        {
            return m_color;
        }

        void Panel::draw(RenderSystem& visitor)
        {
            visitor.drawQuad(m_transformId, m_color, m_M[3][2]);

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