#include "CheckBox.hpp"

#include <iostream>
#include <vector>

namespace crisp
{
    namespace gui
    {
        CheckBox::CheckBox(RenderSystem* renderSystem)
            : m_state(State::Idle)
            , m_isChecked(false)
            , m_prevCheckedState(true)
            , m_label(std::make_unique<Label>(renderSystem, "CheckBox"))
        {
            m_position = glm::vec2(300, 300);
            m_size = glm::vec2(16, 16);
            m_M = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_size, 1));

            m_label->setParent(this);
            m_label->setPosition(glm::vec2(m_size.x + 3.0f, m_size.y - 2.0f));

            m_renderSystem = renderSystem;
            m_transformId = m_renderSystem->registerTransformResource();
            m_texCoordResourceId = m_renderSystem->registerTexCoordResource();
        }

        CheckBox::~CheckBox()
        {
            m_renderSystem->unregisterTransformResource(m_transformId);
            m_renderSystem->unregisterTexCoordResource(m_texCoordResourceId);
        }

        bool CheckBox::isChecked() const
        {
            return m_isChecked;
        }

        void CheckBox::setCheckCallback(std::function<void(bool)> checkCallback)
        {
            m_checkCallback = checkCallback;
        }

        void CheckBox::setText(const std::string& text)
        {
            m_label->setText(text);
        }

        glm::vec2 CheckBox::getSize() const
        {
            return m_size + glm::vec2(m_label->getTextExtent().x + 3.0f, 0.0f);
        }

        ColorPalette CheckBox::getColor() const
        {
            if (m_state == State::Idle)
                return Green;
            else if (m_state == State::Hover)
                return LightGreen;
            else
                return DarkGreen;
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
        }

        void CheckBox::onMouseExited()
        {
            if (m_state == State::Pressed || m_state == State::Idle)
                return;

            m_state = State::Idle;
        }

        void CheckBox::onMousePressed(float x, float y)
        {
            m_state = State::Pressed;
        }

        void CheckBox::onMouseReleased(float x, float y)
        {
            if (getAbsoluteBounds().contains(x, y))
            {
                m_isChecked = !m_isChecked;
                if (m_checkCallback != nullptr)
                    m_checkCallback(m_isChecked);

                if (!m_isChecked)
                    m_renderSystem->updateTexCoordResource(m_texCoordResourceId, glm::vec2(0.0f, 0.0f), glm::vec2(0.5f, 1.0f));
                else
                    m_renderSystem->updateTexCoordResource(m_texCoordResourceId, glm::vec2(0.5f, 0.0f), glm::vec2(1.0f, 1.0f));

                m_state = State::Hover;
            }
            else
            {
                m_state = State::Idle;
            }
        }

        void CheckBox::validate()
        {
            //m_M = glm::translate(glm::vec3(m_absolutePosition, m_depth)) * glm::scale(glm::vec3(m_size, 1.0f));
            //m_renderSystem->updateTransformResource(m_transformId, m_M);
            //
            //m_label->applyParentProperties();
            //m_label->validate();
            //m_label->setValidationStatus(true);
            //
            //if (m_prevCheckedState != m_isChecked)
            //{
            //    if (!m_isChecked)
            //        m_renderSystem->updateTexCoordResource(m_texCoordResourceId, glm::vec2(0.0f, 0.0f), glm::vec2(0.5f, 1.0f));
            //    else
            //        m_renderSystem->updateTexCoordResource(m_texCoordResourceId, glm::vec2(0.5f, 0.0f), glm::vec2(1.0f, 1.0f));
            //    m_prevCheckedState = m_isChecked;
            //}
        }

        void CheckBox::draw(RenderSystem& visitor)
        {
            visitor.draw(*this);

            if (m_state == State::Idle)
                m_label->setColor(Green);
            else if (m_state == State::Hover)
                m_label->setColor(LightGreen);
            else
                m_label->setColor(DarkGreen);
            //visitor.draw(*m_label);
        }

        unsigned int CheckBox::getTexCoordResourceId() const
        {
            return m_texCoordResourceId;
        }
    }
}