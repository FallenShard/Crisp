#include "CheckBox.hpp"

#include <iostream>
#include <vector>

#include "Form.hpp"

namespace crisp
{
    namespace gui
    {
        CheckBox::CheckBox(Form* parentForm)
            : Control(parentForm)
            , m_state(State::Idle)
            , m_isChecked(false)
            , m_label(std::make_unique<Label>(parentForm, "CheckBox"))
        {
            m_widthSizingBehavior = Sizing::Fixed;
            m_heightSizingBehavior = Sizing::Fixed;

            m_size = glm::vec2(16.0f, 16.0f);

            m_transformId = m_renderSystem->registerTransformResource();
            m_M           = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_size, 1.0f));

            m_color   = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            m_colorId = m_renderSystem->registerColorResource();
            m_renderSystem->updateColorResource(m_colorId, m_color);

            m_texCoordResourceId = m_renderSystem->registerTexCoordResource();
            updateTexCoordResource();

            m_label->setParent(this);
            m_label->setPosition({ m_size.x + 5.0f, 1.0f });
            m_label->setAnchor(Anchor::CenterVertically);
        }

        CheckBox::~CheckBox()
        {
            m_renderSystem->unregisterTransformResource(m_transformId);
            m_renderSystem->unregisterTexCoordResource(m_texCoordResourceId);
            m_renderSystem->unregisterColorResource(m_transformId);
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
            return m_size + glm::vec2(m_label->getTextExtent().x + 5.0f, 0.0f);
        }

        Rect<float> CheckBox::getAbsoluteBounds() const
        {
            Rect<float> bounds = { m_M[3][0], m_M[3][1], m_size.x, m_size.y };
            return bounds.merge(m_label->getAbsoluteBounds());
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
            if (getAbsoluteBounds().contains(x, y) && m_state == State::Pressed)
            {
                m_isChecked = !m_isChecked;

                if (m_checkCallback != nullptr)
                    m_checkCallback(m_isChecked);

                updateTexCoordResource();
                m_state = State::Hover;
            }
            else
            {
                m_state = State::Idle;
            }
        }

        void CheckBox::validate()
        {
            if (m_validationFlags & Validation::Transform)
            {
                auto absPos   = getAbsolutePosition();
                auto absDepth = getAbsoluteDepth();

                m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(m_size, 1.0f));
                m_renderSystem->updateTransformResource(m_transformId, m_M);
            }

            if (m_validationFlags & Validation::Color)
            {
                m_color.a = getParentAbsoluteOpacity() * m_opacity;
                m_renderSystem->updateColorResource(m_colorId, m_color);
            }
            
            m_label->setValidationFlags(m_validationFlags);
            m_label->validate();
            m_label->clearValidationFlags();
        }

        void CheckBox::draw(RenderSystem& visitor)
        {
            visitor.drawTexture(m_transformId, m_colorId, m_texCoordResourceId, m_M[3][2]);
            m_label->draw(visitor);
        }

        unsigned int CheckBox::getTexCoordResourceId() const
        {
            return m_texCoordResourceId;
        }

        void CheckBox::updateTexCoordResource()
        {
            if (m_isChecked)
                m_renderSystem->updateTexCoordResource(m_texCoordResourceId, glm::vec4(0.5f, 1.0f, 0.5f, 0.0f));
            else
                m_renderSystem->updateTexCoordResource(m_texCoordResourceId, glm::vec4(0.5f, 1.0f, 0.0f, 0.0f));
        }
    }
}