#include "Control.hpp"

#include <iostream>

namespace crisp
{
    namespace gui
    {
        Control::Control()
            : m_isValidated(false)
            , m_parent(nullptr)
            , m_padding(0.0f)
            , m_depth(-32.0f)
            , m_transformId(-1)
            , m_scale(1.0f)
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
            return m_size;
        }

        void Control::setPadding(glm::vec2&& padding)
        {
            m_padding = padding;
        }

        glm::vec2 Control::getPadding() const
        {
            return m_padding;
        }

        void Control::setDepth(float depth)
        {
            m_depth = depth;
        }

        float Control::getDepth() const
        {
            return m_depth;
        }

        void Control::setScale(float scale)
        {
            m_scale = scale;
        }

        float Control::setScale() const
        {
            return m_scale;
        }

        Rect<float> Control::getAbsoluteBounds() const
        {
            auto size = getSize();
            return{m_absolutePosition.x, m_absolutePosition.y, size.x, size.y};
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

        void Control::invalidate()
        {
            m_isValidated = false;
        }

        bool Control::needsValidation()
        {
            return !m_isValidated;
        }

        void Control::setValidationStatus(bool validationStatus)
        {
            m_isValidated = validationStatus;
        }

        void Control::applyParentProperties()
        {
            m_absolutePosition = m_position;
            if (m_parent)
            {
                m_absolutePosition += m_parent->m_absolutePosition + m_parent->m_padding;
                m_depth = m_parent->m_depth + 1.0f;
            }
        }

        Control* Control::getControlById(const std::string& id)
        {
            return m_id == id ? this : nullptr;
        }

        unsigned int Control::getDistanceToRoot() const
        {
            return m_parent ? 1 + m_parent->getDistanceToRoot() : 0;
        }
        unsigned int Control::getTransformId() const
        {
            return m_transformId;
        }
    }
}