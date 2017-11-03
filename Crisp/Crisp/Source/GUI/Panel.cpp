#include "Panel.hpp"
#include "Form.hpp"

#include <iostream>

namespace crisp::gui
{
    Panel::Panel(Form* parentForm)
        : ControlGroup(parentForm)
        , m_drawComponent(parentForm->getRenderSystem())
    {
        m_color = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    }

    Panel::~Panel()
    {
    }

    void Panel::validate()
    {
        if (m_validationFlags & Validation::Color)
        {
            m_color.a = getParentAbsoluteOpacity() * m_opacity;
            m_drawComponent.update(m_color);
        }

        ControlGroup::validate();

        if (m_validationFlags & Validation::Geometry)
        {
            m_drawComponent.update(m_M);
        }
    }

    void Panel::draw(const RenderSystem& visitor) const
    {
        m_drawComponent.draw(m_M[3][2]);
        ControlGroup::draw(visitor);
    }
        
    void Panel::setClickCallback(std::function<void()> callback)
    {
        m_clickCallback = callback;
    }

    void Panel::onMouseReleased(float x, float y)
    {
        if (m_clickCallback != nullptr && getInteractionBounds().contains(x, y))
        {
            m_clickCallback();
        }

        ControlGroup::onMouseReleased(x, y);
    }
}