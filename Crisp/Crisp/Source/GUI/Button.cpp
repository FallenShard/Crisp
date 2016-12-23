#include "Button.hpp"

#include <iostream>

#include "Application.hpp"

namespace crisp
{
    namespace gui
    {
        Button::Button(RenderSystem* renderSystem)
            : m_state(State::Idle)
            , m_label(std::make_unique<Label>(renderSystem, "Button"))
        {
            m_position = glm::vec2(50.0f, 50.0f);
            m_size = glm::vec2(200.0f, 50.0f);
            m_M = glm::translate(glm::vec3(m_position, m_depth)) * glm::scale(glm::vec3(m_size, 1.0f));

            m_label->setParent(this);
            m_label->setPosition((m_size - m_label->getTextExtent()) / 2.0f);

            m_renderSystem = renderSystem;
            m_transformId = m_renderSystem->registerTransformResource();
        }

        Button::~Button()
        {
            m_renderSystem->unregisterTransformResource(m_transformId);
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
        }

        void Button::onMouseExited()
        {
            if (m_state == State::Pressed || m_state == State::Idle)
                return;

            m_state = State::Idle;
        }

        void Button::onMousePressed(float x, float y)
        {
            m_state = State::Pressed;
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
            m_M = glm::translate(glm::vec3(m_absolutePosition, m_depth)) * glm::scale(glm::vec3(m_size, 1.0f));
            m_renderSystem->updateTransformResource(m_transformId, m_M);

            auto textExtent = m_label->getTextExtent();
            m_label->setPosition((m_size - glm::vec2(textExtent.x, -textExtent.y)) / 2.0f);
            m_label->applyParentProperties();
            m_label->validate();
            m_label->setValidationStatus(true);

            if (m_state == State::Pressed)
                return;

            auto mouse = Application::getMousePosition();
            m_state = getAbsoluteBounds().contains(mouse.x, mouse.y) ? State::Hover : State::Idle;
        }

        void Button::draw(RenderSystem& visitor)
        {
            visitor.draw(*this);

            if (m_state == State::Idle)
                m_label->setColor(Green);
            else if (m_state == State::Hover)
                m_label->setColor(LightGreen);
            else
                m_label->setColor(DarkGreen);
            visitor.draw(*m_label);
        }

        ColorPalette Button::getColor() const
        {
            if (m_state == State::Idle)
                return Gray30;
            else if (m_state == State::Hover)
                return Gray40;
            else
                return Gray20;
        }
    }
}