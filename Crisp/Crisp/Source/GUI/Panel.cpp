#include "Panel.hpp"

namespace crisp
{
    namespace gui
    {
        Panel::Panel(RenderSystem* renderSystem)
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

        ColorPalette Panel::getColor() const
        {
            return DarkGray;
        }

        void Panel::draw(RenderSystem& visitor)
        {
            visitor.draw(*this);

            ControlGroup::draw(visitor);
        }
    }
}