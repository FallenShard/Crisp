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
            m_renderSystem = renderSystem;

            m_transformId = m_renderSystem->registerTransformResource();
            m_M           = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_size, 1.0f));
            
            m_label->setParent(this);
            m_label->setAnchor(Anchor::Center);
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
            if (getAbsoluteBounds().contains(x, y) && m_state == State::Pressed)
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
            if (m_id == "openButton")
            {
                int x = 3;
            }
            auto absPos = getAbsolutePosition();
            auto absDepth = getAbsoluteDepth();
            auto absSize = getSize();

            m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(absSize, 1.0f));
            m_renderSystem->updateTransformResource(m_transformId, m_M);
            
            m_label->validate();
            m_label->setValidationStatus(true);
        }

        void Button::draw(RenderSystem& visitor)
        {
            visitor.drawQuad(m_transformId, getColor(), m_M[3][2]);
            m_label->draw(visitor);
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