#include "CheckBox.hpp"

#include <iostream>
#include <vector>

namespace crisp
{
    namespace
    {
        glm::vec4 hoverBias  (0.3f, 0.3f, 0.3f, 0.0f);
        glm::vec4 pressedBias(-0.5f, -0.5f, -0.5f, 0.0f);
    }

    namespace gui
    {
        CheckBox::CheckBox(const Font& font)
            : m_state(State::Idle)
            , m_isChecked(false)
            , m_label(std::make_unique<Label>(font))
            , m_color(glm::vec4(0.7, 0.7, 0.7, 1))
        {
            m_position = glm::vec2(300, 300);
            m_size = glm::vec2(16, 16);
            m_M = glm::translate(glm::vec3(m_position, 0)) * glm::scale(glm::vec3(m_size, 1));

            m_label->setParent(this);
            m_label->setPosition(glm::vec2(m_size.x + 3.0f, m_size.y - 2.0f));
            m_label->setColor(m_color);
        }

        CheckBox::~CheckBox()
        {
        }

        bool CheckBox::isChecked() const
        {
            return m_isChecked;
        }

        void CheckBox::setText(const std::string& text)
        {
            m_label->setText(text);
        }

        glm::vec2 CheckBox::getSize() const
        {
            return m_size + glm::vec2(m_label->getTextExtent().x + 3.0f, 0.0f);
        }

        void CheckBox::setColor(const glm::vec4& color)
        {
            m_color = color;
        }

        glm::vec4 CheckBox::getColor() const
        {
            return m_color;
        }

        glm::vec4 CheckBox::getRenderedColor() const
        {
            if (m_state == State::Hover)
                return m_color + hoverBias;
            else if (m_state == State::Pressed)
                return m_color + pressedBias;
            else
                return m_color;
        }

        void CheckBox::setCheckCallback(std::function<void(bool)> checkCallback)
        {
            m_checkCallback = checkCallback;
        }

        Rect<float> CheckBox::getBounds() const
        {
            return{m_position.x, m_position.y, };
        }

        void CheckBox::onMouseEntered()
        {
            if (m_state == State::Pressed || m_state == State::Hover)
                return;

            m_state = State::Hover;
            m_label->setColor(m_color + hoverBias);
        }

        void CheckBox::onMouseExited()
        {
            if (m_state == State::Pressed || m_state == State::Idle)
                return;

            m_state = State::Idle;
            m_label->setColor(m_color);
        }

        void CheckBox::onMousePressed(float x, float y)
        {
            m_state = State::Pressed;
            m_label->setColor(m_color + pressedBias);
        }

        void CheckBox::onMouseReleased(float x, float y)
        {
            if (getAbsoluteBounds().contains(x, y))
            {
                m_isChecked = !m_isChecked;
                if (m_checkCallback != nullptr)
                    m_checkCallback(m_isChecked);

                m_state = State::Hover;
                m_label->setColor(m_color + hoverBias);
            }
            else
            {
                m_state = State::Idle;
                m_label->setColor(m_color);
            }
        }

        void CheckBox::validate()
        {
            m_M = glm::translate(glm::vec3(m_absolutePosition, 0)) * glm::scale(glm::vec3(m_size, 1));

            m_label->applyParentProperties();
            m_label->validate();
            m_label->setValidationStatus(true);
        }

        void CheckBox::draw(const DrawingVisitor& visitor) const
        {
            visitor.draw(*this);
            visitor.draw(*m_label);
        }
    }
}