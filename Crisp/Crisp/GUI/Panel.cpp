#include "Panel.hpp"
#include "Form.hpp"

#include <iostream>

namespace crisp::gui
{
Panel::Panel(Form* parentForm)
    : ControlGroup(parentForm)
    , m_drawComponent(parentForm->getRenderSystem())
{
    m_color = glm::vec4(0.125f, 0.125f, 0.125f, 1.0f);
}

Panel::~Panel() {}

void Panel::validate()
{
    ControlGroup::validate();

    m_color.a = getParentAbsoluteOpacity() * m_opacity;
    m_drawComponent.update(m_color);
    m_drawComponent.update(m_M);
}

void Panel::draw(const RenderSystem& renderSystem) const
{
    m_drawComponent.draw(m_M[3][2]);
    // renderSystem.drawDebugRect(getAbsoluteBounds(), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    ControlGroup::draw(renderSystem);
}

void Panel::applyVerticalLayout(float spacing)
{
    float y = 0.0f;
    for (auto& c : m_children)
    {
        c->setPosition({ c->getPosition().x, y });
        y += c->getSize().y + spacing;
    }

    setValidationFlags(Validation::Geometry);
}
} // namespace crisp::gui