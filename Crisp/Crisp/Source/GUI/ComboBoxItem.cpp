#include "ComboBoxItem.hpp"

#include <iostream>

#include "Form.hpp"
#include "Label.hpp"

namespace crisp::gui
{
    ComboBoxItem::ComboBoxItem(Form* parentForm, std::string text)
        : Control(parentForm)
        , m_state(State::Idle)
        , m_stateColors(static_cast<std::size_t>(State::Count))
        , m_label(std::make_unique<Label>(parentForm, text))
        , m_drawComponent(parentForm->getRenderSystem())
    {
        setBackgroundColor(glm::vec3(0.3f));
        setTextColor(glm::vec3(1.0f, 0.6f, 0.0f));
        setPadding({ 3.0f, 3.0f });

        m_M = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_sizeHint, 1.0f));
        m_color = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

        glm::vec4 color = glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity);
        m_colorAnim = std::make_shared<PropertyAnimation<glm::vec4, Easing::SlowOut>>(0.4, color, color, [this](const glm::vec4& t)
        {
            setColor(t);
        });
        m_labelColorAnim = std::make_shared<PropertyAnimation<glm::vec4, Easing::SlowOut>>(0.4, glm::vec4(1.0f), glm::vec4(1.0f), [this](const glm::vec4& t)
        {
            m_label->setColor(t);
        });

        m_label->setParent(this);
        m_label->setAnchor(Anchor::CenterLeft);
    }

    ComboBoxItem::~ComboBoxItem()
    {
        m_form->getAnimator()->remove(m_colorAnim);
        m_form->getAnimator()->remove(m_labelColorAnim);
    }

    const std::string& ComboBoxItem::getText() const
    {
        return m_label->getText();
    }

    void ComboBoxItem::setText(const std::string& text)
    {
        m_label->setText(text);
    }

    void ComboBoxItem::setTextColor(const glm::vec3& color)
    {
        m_label->setColor({ color, 1.0f });
        m_stateColors[static_cast<std::size_t>(State::Idle)].text = color;
        m_stateColors[static_cast<std::size_t>(State::Hover)].text = glm::clamp(color * 1.25f, glm::vec3(0.0f), glm::vec3(1.0f));
        m_stateColors[static_cast<std::size_t>(State::Pressed)].text = color * 0.75f;
        setValidationFlags(Validation::Color);
    }

    void ComboBoxItem::setTextColor(const glm::vec3& color, State state)
    {
        if (state == State::Idle)
            m_label->setColor({ color, 1.0f });

        m_stateColors[static_cast<std::size_t>(state)].text = color;
        setValidationFlags(Validation::Color);
    }

    void ComboBoxItem::setFontSize(unsigned int fontSize)
    {
        m_label->setFontSize(fontSize);
    }

    void ComboBoxItem::setEnabled(bool enabled)
    {
        if (enabled)
            setState(State::Disabled);
        else
            setState(State::Idle);
    }

    void ComboBoxItem::setBackgroundColor(const glm::vec3& color)
    {
        m_color = glm::vec4(color, m_opacity);
        m_stateColors[static_cast<std::size_t>(State::Idle)].background = color;
        m_stateColors[static_cast<std::size_t>(State::Hover)].background = glm::clamp(color * 1.25f, glm::vec3(0.0f), glm::vec3(1.0f));
        m_stateColors[static_cast<std::size_t>(State::Pressed)].background = color * 0.75f;
        setValidationFlags(Validation::Color);
    }

    void ComboBoxItem::setBackgroundColor(const glm::vec3& color, State state)
    {
        if (state == State::Idle)
            m_color = glm::vec4({ color, m_opacity });

        m_stateColors[static_cast<std::size_t>(state)].background = color;
        setValidationFlags(Validation::Color);
    }

    void ComboBoxItem::onMouseEntered(float, float)
    {
        if (m_state == State::Disabled || m_state != State::Idle)
            return;

        setState(State::Hover);
    }

    void ComboBoxItem::onMouseExited(float, float)
    {
        if (m_state == State::Disabled || m_state != State::Hover)
            return;

        setState(State::Idle);
    }

    void ComboBoxItem::onMousePressed(float, float)
    {
        if (m_state == State::Disabled)
            return;

        setState(State::Pressed);
    }

    void ComboBoxItem::onMouseReleased(float x, float y)
    {
        if (m_state == State::Disabled)
            return;

        if (getInteractionBounds().contains(x, y) && m_state == State::Pressed)
            clicked();

        setState(State::Idle);
    }

    void ComboBoxItem::validate()
    {
        if (m_validationFlags & Validation::Geometry)
        {
            auto absPos = getAbsolutePosition();
            auto absDepth = getAbsoluteDepth();
            auto absSize = getSize();

            m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(absSize, 1.0f));
            m_drawComponent.update(m_M);
        }

        if (m_validationFlags & Validation::Color)
        {
            m_color.a = getParentAbsoluteOpacity() * m_opacity;
            m_drawComponent.update(m_color);
        }

        m_label->setValidationFlags(m_validationFlags);
        m_label->validate();
        m_label->clearValidationFlags();
    }

    void ComboBoxItem::draw(const RenderSystem& renderSystem) const
    {
        m_drawComponent.draw(m_M[3][2]);
        m_label->draw(renderSystem);
    }

    void ComboBoxItem::setState(State state)
    {
        if (m_state == state)
            return;

        m_state = state;
        glm::vec4 targetColor = glm::vec4(m_stateColors[static_cast<std::size_t>(m_state)].background, m_opacity);

        m_colorAnim->reset(glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity), targetColor);
        if (!m_colorAnim->isActive())
            m_form->getAnimator()->add(m_colorAnim);

        m_labelColorAnim->reset(m_label->getColor(), glm::vec4(m_stateColors[static_cast<std::size_t>(m_state)].text, m_opacity));
        if (!m_labelColorAnim->isActive())
            m_form->getAnimator()->add(m_labelColorAnim);
    }
}