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
            , m_useAbsoluteSizing(false)
        {
        }

        void ControlGroup::useAbsoluteSizing(bool absoluteSizing)
        {
            m_useAbsoluteSizing = absoluteSizing;
        }

        glm::vec2 ControlGroup::getSize() const
        {
            if (m_useAbsoluteSizing)
            {
                return m_size;
            }
            else
            {
                glm::vec2 childrenSize = glm::vec2(0.0f, 0.0f);

                for (auto& child : m_children)
                {
                    if (child->needsValidation())
                    {
                        child->applyParentProperties();
                        child->validate();
                        child->setValidationStatus(true);
                    }

                    auto childPos = child->getPosition();
                    auto childSize = child->getSize();
                    childrenSize.x = std::max(childrenSize.x, childPos.x + childSize.x);
                    childrenSize.y = std::max(childrenSize.y, childPos.y + childSize.y);
                }

                return childrenSize + 2.0f * m_padding;
            }
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

        bool ControlGroup::needsValidation()
        {
            if (!m_isValidated)
                return true;

            bool childrenValid = false;
            for (auto& child : m_children)
                childrenValid |= child->needsValidation();

            return childrenValid;
        }

        void ControlGroup::validate()
        {
            auto size = getSize();

            m_M = glm::translate(glm::vec3(m_absolutePosition, 0.0f)) * glm::scale(glm::vec3(size, 1.0f));
        }

        void ControlGroup::draw(const DrawingVisitor& visitor) const
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