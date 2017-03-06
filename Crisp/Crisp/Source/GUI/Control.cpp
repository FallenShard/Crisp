#include "Control.hpp"

#include <iostream>

namespace crisp
{
    namespace gui
    {
        Control::Control()
            : m_id("")
            , m_parent(nullptr)
            , m_anchor(Anchor::TopLeft)
            , m_widthSizingBehavior(Sizing::Fixed)
            , m_heightSizingBehavior(Sizing::Fixed)
            , m_parentSizePercent(1.0f, 1.0f)
            , m_position(0.0f)
            , m_size(50.0f)
            , m_padding(0.0f)
            , m_depthOffset(1.0f)
            , m_opacity(1.0f)
            , m_scale(1.0f)
            , m_isValidated(false)
            , m_transformId(-1)
        {
        }

        Control::~Control()
        {
        }

        void Control::setId(std::string id)
        {
            m_id = id;
        }

        std::string Control::getId() const
        {
            return m_id;
        }

        void Control::setParent(Control* parent)
        {
            m_parent = parent;
            invalidate();
        }

        Control* Control::getParent() const
        {
            return m_parent;
        }

        void Control::setAnchor(Anchor anchor)
        {
            m_anchor = anchor;
            invalidate();
        }

        Anchor Control::getAnchor() const
        {
            return m_anchor;
        }

        void Control::setWidthSizingBehavior(Sizing sizing, float widthPercent)
        {
            m_widthSizingBehavior = sizing;
            m_parentSizePercent.x = widthPercent;
            invalidate();
        }

        Sizing Control::getWidthSizingBehavior() const
        {
            return m_widthSizingBehavior;
        }

        void Control::setHeightSizingBehavior(Sizing sizing, float heightPercent)
        {
            m_heightSizingBehavior = sizing;
            m_parentSizePercent.y = heightPercent;
            invalidate();
        }

        Sizing Control::getHeightSizingBehavior() const
        {
            return m_heightSizingBehavior;
        }

        void Control::setPosition(const glm::vec2& position)
        {
            m_position = position;
            invalidate();
        }

        glm::vec2 Control::getPosition() const
        {
            return m_position;
        }

        void Control::setSize(const glm::vec2& size)
        {
            m_size = size;
            invalidate();
        }

        glm::vec2 Control::getSize() const
        {
            return { getWidth(), getHeight() };
        }

        float Control::getWidth() const
        {
            if (!m_parent)
                return m_size.x;

            if (m_widthSizingBehavior == Sizing::FillParent)
            {
                return (m_parent->getWidth() - 2.0f * getParentPadding().x) * m_parentSizePercent.x;
            }
            else // Sizing::Fixed
            {
                // If parent is not fixed, use our fixed size
                if (m_parent->getWidthSizingBehavior() != Sizing::Fixed)
                {
                    return m_size.x;
                }
                else // Else, we have to limit ourselves to parent size and padding
                {
                    return std::min(m_size.x, m_parent->getWidth() - 2.0f * getParentPadding().x);
                }
            }
        }

        float Control::getHeight() const
        {
            if (!m_parent)
                return m_size.y;

            if (m_heightSizingBehavior == Sizing::FillParent)
            {
                return (m_parent->getHeight() - 2.0f * getParentPadding().y) * m_parentSizePercent.y;
            }
            else // Sizing::Fixed
            {
                // If parent is not fixed, use our fixed size
                if (m_parent->getHeightSizingBehavior() != Sizing::Fixed)
                {
                    return m_size.y;
                }
                else // Else, we have to limit ourselves to parent size and padding
                {
                    return std::min(m_size.y, m_parent->getHeight() - 2.0f * getParentPadding().y);
                }
            }
        }

        void Control::setPadding(glm::vec2&& padding)
        {
            m_padding = padding;
            invalidate();
        }

        glm::vec2 Control::getPadding() const
        {
            return m_padding;
        }

        void Control::setDepthOffset(float depthOffset)
        {
            m_depthOffset = depthOffset;
            invalidate();
        }

        float Control::getDepthOffset() const
        {
            return m_depthOffset;
        }

        void Control::setScale(float scale)
        {
            m_scale = scale;
            invalidate();
        }

        float Control::setScale() const
        {
            return m_scale;
        }

        Rect<float> Control::getAbsoluteBounds() const
        {
            return{ m_M[3][0], m_M[3][1], m_M[0][0], m_M[1][1] };
        }

        const glm::mat4& Control::getModelMatrix() const
        {
            return m_M;
        }

        void Control::onMouseMoved(float x, float y)
        {
        }

        void Control::onMouseEntered()
        {
        }

        void Control::onMouseExited()
        {
        }

        void Control::onMousePressed(float x, float y)
        {
        }

        void Control::onMouseReleased(float x, float y)
        {
        }

        void Control::setValidationStatus(bool validationStatus)
        {
            m_isValidated = validationStatus;
        }

        void Control::invalidate()
        {
            m_isValidated = false;
        }

        bool Control::isInvalidated()
        {
            return !m_isValidated;
        }

        void Control::validate()
        {
            auto absPos = getParentAbsolutePosition() + m_position;
            auto absDepth = getParentAbsoluteDepth() + m_depthOffset;
            m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(m_size, 1.0f));
        }

        Control* Control::getControlById(const std::string& id)
        {
            return m_id == id ? this : nullptr;
        }

        unsigned int Control::getTransformId() const
        {
            return m_transformId;
        }

        unsigned int Control::getRootDistance() const
        {
            return m_parent ? m_parent->getRootDistance() + 1 : 0;
        }

        void Control::printDebugId() const
        {
            for (unsigned int i = 0; i < getRootDistance(); i++)
                std::cout << "    ";

            std::cout << m_id << std::endl;
        }

        glm::vec2 Control::getParentAbsolutePosition() const
        {
            return m_parent ? glm::vec2(m_parent->m_M[3][0], m_parent->m_M[3][1]) : glm::vec2(0.0f);
        }

        float Control::getParentAbsoluteDepth() const
        {
            return m_parent ? m_parent->m_M[3][2] : 0.0f;
        }

        glm::vec2 Control::getParentPadding() const
        {
            return m_parent ? m_parent->getPadding() : glm::vec2(0.0f);
        }

        glm::vec2 Control::getAbsolutePosition() const
        {
            auto parentAbsPos = getParentAbsolutePosition();
            auto parentPadding = getParentPadding();
            switch (m_anchor)
            {
            default:
            case Anchor::TopLeft:
            {
                return parentAbsPos + glm::max(parentPadding, m_position);
            }

            case Anchor::BottomRight:
            {
                auto parentSize = m_parent ? m_parent->getSize() : glm::vec2(0.0f);
                auto size = getSize();

                return parentAbsPos + parentPadding + parentSize - size - m_position;
            }

            case Anchor::BottomLeft:
            {
                auto parentSize = m_parent ? m_parent->getSize() : glm::vec2(0.0f);

                auto size = getSize();

                auto absPos = parentAbsPos;
                absPos.x += std::max(m_position.x, parentPadding.x);
                absPos.y += parentSize.y - parentPadding.y - size.y - m_position.y;

                return absPos;
            }

            case Anchor::TopRight:
            {
                auto parentSize = m_parent ? m_parent->getSize() : glm::vec2(0.0f);

                auto size = getSize();

                auto absPos = parentAbsPos;
                absPos.x += parentSize.x - size.x - m_position.x;
                absPos.y += m_position.y;

                return absPos + parentPadding;
            }

            case Anchor::Center:
            {
                auto parentSize = m_parent ? m_parent->getSize() : glm::vec2(0.0f);
                auto size = getSize();

                auto absPos = parentAbsPos;

                if (m_widthSizingBehavior == Sizing::FillParent)
                    absPos.x += parentPadding.x;
                else
                    absPos.x += (parentSize.x - size.x) / 2.0f;

                if (m_heightSizingBehavior == Sizing::FillParent)
                    absPos.y += parentPadding.y;
                else
                    absPos.y += (parentSize.y - size.y) / 2.0f;

                return absPos;
            }
            }
        }

        float Control::getAbsoluteDepth() const
        {
            return getParentAbsoluteDepth() + m_depthOffset;
        }
    }
}