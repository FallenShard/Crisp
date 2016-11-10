#include "Button.hpp"

#include <iostream>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Application.hpp"

namespace crisp
{
    namespace gui
    {
        namespace
        {
            glm::vec4 color(0.7f, 0.7f, 0.7f, 1.0f);
            glm::vec4 hover(0.3f, 0.3f, 0.3f, 0.0f);
            glm::vec4 press(-0.5f, -0.5f, -0.5f, 0.0f);
        }

        Button::Button(const Font& font)
            : m_state(State::Idle)
            , m_label(std::make_unique<Label>(font, "Button"))
        {
            m_position = glm::vec2(50.0f, 50.0f);
            m_size = glm::vec2(200.0f, 50.0f);
            m_M = glm::translate(glm::vec3(m_position, 0.0f)) * glm::scale(glm::vec3(m_size, 1.0f));

            m_label->setParent(this);
            m_label->setPosition((m_size - m_label->getTextExtent()) / 2.0f);
            m_label->setColor(color);
        }

        Button::~Button()
        {
        }

        void Button::setClickCallback(std::function<void()> callback)
        {
            m_clickCallback = callback;
        }

        void Button::setText(const std::string& text)
        {
            m_label->setText(text);
            
        }

        void Button::onMouseEntered()
        {
            if (m_state == State::Pressed || m_state == State::Hover)
                return;

            m_state = State::Hover;
            m_label->setColor(color + hover);
        }

        void Button::onMouseExited()
        {
            std::cout << "Exited" << std::endl;
            if (m_state == State::Pressed || m_state == State::Idle)
                return;

            m_state = State::Idle;
            m_label->setColor(color);
        }

        void Button::onMousePressed(float x, float y)
        {
            m_state = State::Pressed;
            m_label->setColor(color + press);
        }

        void Button::onMouseReleased(float x, float y)
        {
            if (getAbsoluteBounds().contains(x, y))
            {
                if (m_clickCallback != nullptr)
                    m_clickCallback();

                m_state = State::Hover;
            }
            else
            {
                m_state = State::Idle;
            }
        }

        void Button::validate()
        {
            m_M = glm::translate(glm::vec3(m_absolutePosition, 0.0f)) * glm::scale(glm::vec3(m_size, 1.0f));

            if (m_state == State::Pressed)
                return;

            auto mouse = Application::getMousePosition();
            if (getAbsoluteBounds().contains(mouse.x, mouse.y))
                m_state = State::Hover;
            else
                m_state = State::Idle;

            m_label->setPosition((m_size - m_label->getTextExtent()) / 2.0f + glm::vec2(0.0f, 6.0f));
            m_label->applyParentProperties();
            m_label->validate();
            m_label->setValidationStatus(true);
        }

        void Button::draw(const DrawingVisitor& visitor) const
        {
            m_label->setColor(color + getColorOffset());
            visitor.draw(*this);
            visitor.draw(*m_label);
        }

        glm::vec4 Button::getColorOffset() const
        {
            if (m_state == State::Idle)
                return glm::vec4(0.0f);
            else if (m_state == State::Hover)
                return hover;
            else
                return press;
        }
    }
}