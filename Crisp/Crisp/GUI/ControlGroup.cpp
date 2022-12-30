#define NOMINMAX
#include "ControlGroup.hpp"

#include <algorithm>
#include <iostream>

#include <Crisp/GUI/Form.hpp>

namespace crisp::gui
{
ControlGroup::ControlGroup(Form* parentForm)
    : Control(parentForm)
    , m_prevMousePos(-1.0f)
{
}

ControlGroup::~ControlGroup() {}

void ControlGroup::addControl(std::unique_ptr<Control> control)
{
    m_form->postGuiUpdate(
        [this, cc = control.release()]() mutable
        {
            std::unique_ptr<Control> con = std::unique_ptr<Control>(cc);
            addControlDirect(std::move(con));
        });
}

void ControlGroup::addControlDirect(std::unique_ptr<Control> control)
{
    control->setParent(this);
    m_children.push_back(std::move(control));
    setValidationFlags(Validation::All);
}

void ControlGroup::removeControl(const std::string& id)
{
    m_form->postGuiUpdate(
        [this, id]()
        {
            m_children.erase(
                std::remove_if(
                    m_children.begin(),
                    m_children.end(),
                    [id](const std::unique_ptr<Control>& child)
                    {
                        return child->getId() == id;
                    }),
                m_children.end());
            setValidationFlags(Validation::All);
        });
}

void ControlGroup::clearControls()
{
    m_form->postGuiUpdate(
        [this]()
        {
            m_children.clear();
        });
}

float ControlGroup::getWidth() const
{
    if (m_horizontalSizingPolicy == SizingPolicy::WrapContent)
    {
        float childrenWidth = 0.0f;
        for (auto& child : m_children)
        {
            if (child->getHorizontalSizingPolicy() == SizingPolicy::FillParent)
                continue;

            auto childPosX = child->getPosition().x;
            auto childWidth = child->getWidth();
            childrenWidth = std::max(childrenWidth, childPosX + childWidth);
        }

        return childrenWidth + m_paddingX[0] + m_paddingX[1];
    }
    else
    {
        return Control::getWidth();
    }
}

float ControlGroup::getHeight() const
{
    if (m_verticalSizingPolicy == SizingPolicy::WrapContent)
    {
        float childrenHeight = 0.0f;
        for (auto& child : m_children)
        {
            if (child->getVerticalSizingPolicy() == SizingPolicy::FillParent)
                continue;

            auto childPosY = child->getPosition().y;
            auto childHeight = child->getHeight();
            childrenHeight = std::max(childrenHeight, childPosY + childHeight);
        }

        return childrenHeight + m_paddingY[0] + m_paddingY[1];
    }
    else
    {
        return Control::getHeight();
    }
}

Rect<float> ControlGroup::getInteractionBounds() const
{
    Rect<float> interactionBounds = getAbsoluteBounds();
    for (auto& child : m_children)
    {
        Rect<float> childBounds = child->getInteractionBounds();
        interactionBounds = interactionBounds.merge(childBounds);
    }

    return interactionBounds;
}

void ControlGroup::onMouseMoved(float x, float y)
{
    for (auto& child : m_children)
    {
        auto bounds = child->getInteractionBounds();

        bool containsPrevCoords = bounds.contains(m_prevMousePos.x, m_prevMousePos.y);
        bool containsCurrCoords = bounds.contains(x, y);

        if (!containsPrevCoords && containsCurrCoords)
            child->onMouseEntered(x, y);

        if (containsPrevCoords && containsCurrCoords)
            child->onMouseMoved(x, y);

        if (containsPrevCoords && !containsCurrCoords)
            child->onMouseExited(x, y);
    }

    m_prevMousePos = {x, y};
}

void ControlGroup::onMouseEntered(float x, float y)
{
    for (auto& child : m_children)
    {
        auto bounds = child->getInteractionBounds();

        bool containsPrevCoords = bounds.contains(m_prevMousePos.x, m_prevMousePos.y);
        bool containsCurrCoords = bounds.contains(x, y);

        if (!containsPrevCoords && containsCurrCoords)
            child->onMouseEntered(x, y);
    }

    m_prevMousePos = {x, y};
}

void ControlGroup::onMouseExited(float x, float y)
{
    for (auto& child : m_children)
    {
        child->onMouseExited(x, y);
    }

    m_prevMousePos = {-1.0f, -1.0f};
}

bool ControlGroup::onMousePressed(float x, float y)
{
    for (auto& child : m_children)
    {
        if (child->getInteractionBounds().contains(x, y))
        {
            if (child->onMousePressed(x, y))
                return true;
        }
    }

    return false;
}

bool ControlGroup::onMouseReleased(float /*x*/, float /*y*/)
{
    // for (auto& child : m_children)
    //{
    //     if (child->onMouseReleased(x, y))
    //         return true;
    // }

    return false;
}

void ControlGroup::validate()
{
    if (m_layoutType == LayoutType::Vertical)
    {
        float y = 0.0f;
        for (auto& child : m_children)
        {
            child->setPositionY(y);
            y += child->getHeight() + m_spacing;
        }
    }
    else if (m_layoutType == LayoutType::Horizontal)
    {
        float x = 0.0f;
        for (auto& child : m_children)
        {
            child->setPositionX(x);
            x += child->getWidth() + m_spacing;
        }
    }

    auto size = getSize();
    auto absPos = getAbsolutePosition();
    auto absDepth = getAbsoluteDepth();
    m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(size, 1.0f));

    for (auto& child : m_children)
        child->validateAndClearFlags();
}

void ControlGroup::draw(const RenderSystem& visitor) const
{
    for (auto& child : m_children)
    {
        child->draw(visitor);
    }
}

void ControlGroup::printDebugId() const
{
    Control::printDebugId();

    for (auto& c : m_children)
    {
        c->printDebugId();
    }
}

Control* ControlGroup::getControlById(const std::string& id)
{
    if (m_id == id)
        return this;

    for (auto& child : m_children)
    {
        auto foundControl = child->getControlById(id);
        if (foundControl)
            return foundControl;
    }

    return nullptr;
}

void ControlGroup::visit(std::function<void(Control*)> func)
{
    func(this);

    for (auto& child : m_children)
        child->visit(func);
}
} // namespace crisp::gui