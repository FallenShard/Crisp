#include "Control.hpp"

#include <iostream>

#include "Form.hpp"

namespace crisp
{
    namespace gui
    {
        Control::Control(Form* form)
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
            , m_color(glm::vec4(1.0f))
            , m_opacity(1.0f)
            , m_scale(1.0f)
            , m_validationFlags(Validation::All)
            , m_transformId(-1)
            , m_form(form)
            , m_renderSystem(form->getRenderSystem())
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
            setValidationFlags(Validation::All);
        }

        Control* Control::getParent() const
        {
            return m_parent;
        }

        void Control::setAnchor(Anchor anchor)
        {
            m_anchor = anchor;
            setValidationFlags(Validation::Transform);
        }

        Anchor Control::getAnchor() const
        {
            return m_anchor;
        }

        void Control::setWidthSizingBehavior(Sizing sizing, float widthPercent)
        {
            m_widthSizingBehavior = sizing;
            m_parentSizePercent.x = widthPercent;
            setValidationFlags(Validation::Transform);
        }

        Sizing Control::getWidthSizingBehavior() const
        {
            return m_widthSizingBehavior;
        }

        void Control::setHeightSizingBehavior(Sizing sizing, float heightPercent)
        {
            m_heightSizingBehavior = sizing;
            m_parentSizePercent.y = heightPercent;
            setValidationFlags(Validation::Transform);
        }

        Sizing Control::getHeightSizingBehavior() const
        {
            return m_heightSizingBehavior;
        }

        void Control::setPosition(const glm::vec2& position)
        {
            m_position = position;
            setValidationFlags(Validation::Transform);
        }

        glm::vec2 Control::getPosition() const
        {
            return m_position;
        }

        void Control::setSize(const glm::vec2& size)
        {
            m_size = size;
            setValidationFlags(Validation::Transform);
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
            setValidationFlags(Validation::Transform);
        }

        glm::vec2 Control::getPadding() const
        {
            return m_padding;
        }

        void Control::setDepthOffset(float depthOffset)
        {
            m_depthOffset = depthOffset;
            setValidationFlags(Validation::Transform);
        }

        float Control::getDepthOffset() const
        {
            return m_depthOffset;
        }

        void Control::setScale(float scale)
        {
            m_scale = scale;
            setValidationFlags(Validation::Transform);
        }

        float Control::setScale() const
        {
            return m_scale;
        }

        void Control::setColor(glm::vec4 color)
        {
            m_color.r = color.r;
            m_color.g = color.g;
            m_color.b = color.b;
            m_opacity = color.a;
            setValidationFlags(Validation::Color);
        }

        glm::vec4 Control::getColor() const
        {
            return m_color;
        }

        void Control::setOpacity(float opacity)
        {
            m_opacity = opacity;
            setValidationFlags(Validation::Color);
        }

        float Control::getOpacity() const
        {
            return m_opacity;
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

        void Control::setValidationFlags(Validation validation)
        {
            m_validationFlags = m_validationFlags | validation;
        }

        void Control::clearValidationFlags()
        {
            m_validationFlags = Validation::None;
        }

        Validation Control::getValidationFlags() const
        {
            return m_validationFlags;
        }

        bool Control::needsValidation()
        {
            return m_validationFlags != Validation::None;
        }

        void Control::validate()
        {
            if (m_validationFlags & Validation::Transform)
            {
                auto absPos = getParentAbsolutePosition() + m_position;
                auto absDepth = getParentAbsoluteDepth() + m_depthOffset;
                m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(m_size, 1.0f));
            }

            if (m_validationFlags & Validation::Color)
            {
                m_color.a = getParentAbsoluteOpacity() * m_opacity;
            }
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

        float Control::getParentAbsoluteOpacity() const
        {
            return m_parent ? m_parent->m_color.a : 1.0f;
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
                return parentAbsPos + parentPadding + m_position;
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

            case Anchor::CenterHorizontally:
            {
                auto parentSize = m_parent ? m_parent->getSize() : glm::vec2(0.0f);
                auto size = getSize();

                auto absPos = parentAbsPos;

                if (m_widthSizingBehavior == Sizing::FillParent)
                    absPos.x += parentPadding.x;
                else
                    absPos.x += (parentSize.x - size.x) / 2.0f;

                absPos.y += parentPadding.y + m_position.y;

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