#define NOMINMAX
#include "ControlGroup.hpp"

#include <iostream>

#include <algorithm>

namespace crisp
{
    namespace gui
    {
        ControlGroup::ControlGroup()
            : m_prevMousePos(-1.0, -1.0)
        {
        }

        void ControlGroup::addControl(std::shared_ptr<Control> control)
        {
            m_children.emplace_back(control);
            control->setParent(this);

            invalidate();
        }

        void ControlGroup::removeControl(const std::string& id)
        {
            std::remove_if(std::begin(m_children), std::end(m_children), [id](const std::shared_ptr<Control>& child)
            {
                return child->getId() == id;
            });
        }

        float ControlGroup::getWidth() const
        {
            if (m_widthSizingBehavior == Sizing::WrapContent)
            {
                float childrenWidth = 0.0f;
                for (auto& child : m_children)
                {
                    auto childPosX = child->getPosition().x;
                    auto childWidth = child->getWidth();
                    childrenWidth = std::max(childrenWidth, childPosX + childWidth);
                }

                return childrenWidth + m_padding.x;
            }
            else
            {
                return Control::getWidth();
            }
        }

        float ControlGroup::getHeight() const
        {
            if (m_heightSizingBehavior == Sizing::WrapContent)
            {
                float childrenHeight = 0.0f;
                for (auto& child : m_children)
                {
                    auto childPosY = child->getPosition().y;
                    auto childHeight = child->getHeight();
                    childrenHeight = std::max(childrenHeight, childPosY + childHeight);
                }

                return childrenHeight + m_padding.y;
            }
            else
            {
                return Control::getHeight();
            }
        }

        void ControlGroup::onMouseMoved(float x, float y)
        {
            for (auto& child : m_children)
            {
                auto bounds = child->getAbsoluteBounds();

                bool containsPrevCoords = bounds.contains(m_prevMousePos.x, m_prevMousePos.y);
                bool containsCurrCoords = bounds.contains(x, y);

                if (!containsPrevCoords && containsCurrCoords)
                    child->onMouseEntered();

                if (containsPrevCoords && containsCurrCoords)
                    child->onMouseMoved(x, y);

                if (containsPrevCoords && !containsCurrCoords)
                    child->onMouseExited();
            }

            m_prevMousePos.x = x;
            m_prevMousePos.y = y;
        }

        void ControlGroup::onMouseEntered()
        {

        }

        void ControlGroup::onMouseExited()
        {
            for (auto& child : m_children)
            {
                child->onMouseExited();
            }
        }

        void ControlGroup::onMousePressed(float x, float y)
        {
            for (auto& child : m_children)
            {
                if (child->getAbsoluteBounds().contains(x, y))
                    child->onMousePressed(x, y);
            }
        }

        void ControlGroup::onMouseReleased(float x, float y)
        {
            for (auto& child : m_children)
            {
                child->onMouseReleased(x, y);
            }
        }

        void ControlGroup::invalidate()
        {
            m_isValidated = false;
            
            for (auto& child : m_children)
                child->invalidate();
        }

        bool ControlGroup::isInvalidated()
        {
            if (!m_isValidated)
                return true;

            bool childrenValid = false;
            for (auto& child : m_children)
                childrenValid |= child->isInvalidated();

            return childrenValid;
        }

        void ControlGroup::validate()
        {
            auto size = getSize();
            auto absPos = getAbsolutePosition();
            auto absDepth = getAbsoluteDepth();

            m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(size, 1.0f));

            for (auto& child : m_children)
            {
                if (child->isInvalidated())
                {
                    child->validate();
                    child->setValidationStatus(true);
                }
            }
        }

        void ControlGroup::draw(RenderSystem& visitor)
        {
            for (auto& child : m_children)
                child->draw(visitor);
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
    }
}