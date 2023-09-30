#include "CheckBox.hpp"

#include <iostream>
#include <vector>

#include "Form.hpp"
#include "Label.hpp"

namespace crisp::gui {
CheckBox::CheckBox(Form* parentForm)
    : Control(parentForm)
    , m_state(State::Idle)
    , m_isChecked(false)
    , m_label(std::make_unique<Label>(parentForm, "CheckBox"))
    , m_hoverColor(0.4f, 0.4f, 0.4f)
    , m_idleColor(0.3f, 0.3f, 0.3f)
    , m_pressedColor(0.2f, 0.2f, 0.2f)
    , m_disabledColor(0.65f, 0.65f, 0.65f) {
    m_sizeHint = glm::vec2(16.0f, 16.0f);

    m_transformId = m_renderSystem->registerTransformResource();
    m_M = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_sizeHint, 1.0f));

    m_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    m_colorId = m_renderSystem->registerColorResource();
    m_renderSystem->updateColorResource(m_colorId, m_color);

    m_texCoordResourceId = m_renderSystem->registerTexCoordResource();
    updateTexCoordResource();

    glm::vec4 color = glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity);
    m_colorAnim = std::make_shared<PropertyAnimation<glm::vec4, Easing::SlowOut>>(
        0.5, color, color, [this](const glm::vec4& t) { setColor(t); });

    m_label->setParent(this);
    m_label->setPosition({m_sizeHint.x + 5.0f, 1.0f});
    m_label->setAnchor(Anchor::CenterLeft);
    m_label->setOrigin(Origin::CenterLeft);
}

CheckBox::~CheckBox() {
    m_form->getAnimator()->remove(m_colorAnim);
    m_renderSystem->unregisterTransformResource(m_transformId);
    m_renderSystem->unregisterTexCoordResource(m_texCoordResourceId);
    m_renderSystem->unregisterColorResource(m_colorId);
}

bool CheckBox::isChecked() const {
    return m_isChecked;
}

void CheckBox::setChecked(bool isChecked) {
    m_isChecked = isChecked;
    checked(m_isChecked);
    updateTexCoordResource();
}

void CheckBox::setText(const std::string& text) {
    m_label->setText(text);
}

void CheckBox::setEnabled(bool enabled) {
    if (enabled) {
        setState(State::Disabled);
    } else {
        setState(State::Idle);
    }
}

void CheckBox::setIdleColor(const glm::vec3& color) {
    m_idleColor = color;
    setValidationFlags(Validation::Color);
}

void CheckBox::setPressedColor(const glm::vec3& color) {
    m_pressedColor = color;
    setValidationFlags(Validation::Color);
}

void CheckBox::setHoverColor(const glm::vec3& color) {
    m_hoverColor = color;
    setValidationFlags(Validation::Color);
}

glm::vec2 CheckBox::getSize() const {
    return m_sizeHint + glm::vec2(m_label->getTextExtent().x + 5.0f, 0.0f);
}

Rect<float> CheckBox::getAbsoluteBounds() const {
    Rect<float> bounds = {m_M[3][0], m_M[3][1], m_sizeHint.x, m_sizeHint.y};
    return bounds.merge(m_label->getAbsoluteBounds());
}

void CheckBox::onMouseEntered(float /*x*/, float /*y*/) {
    if (m_state == State::Disabled || m_state != State::Idle) {
        return;
    }

    setState(State::Hover);
}

void CheckBox::onMouseExited(float /*x*/, float /*y*/) {
    if (m_state == State::Disabled || m_state != State::Hover) {
        return;
    }

    setState(State::Idle);
}

bool CheckBox::onMousePressed(float /*x*/, float /*y*/) {
    if (m_state == State::Disabled) {
        return false;
    }

    setState(State::Pressed);
    m_form->setFocusedControl(this);
    return true;
}

bool CheckBox::onMouseReleased(float x, float y) {
    if (m_state == State::Disabled) {
        return false;
    }

    if (getAbsoluteBounds().contains(x, y) && m_state == State::Pressed) {
        setChecked(!m_isChecked);
        setState(State::Hover);
    } else {
        setState(State::Idle);
    }

    m_form->setFocusedControl(nullptr);
    return true;
}

void CheckBox::validate() {
    auto absPos = getAbsolutePosition();
    auto absDepth = getAbsoluteDepth();

    m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(m_sizeHint, 1.0f));
    m_renderSystem->updateTransformResource(m_transformId, m_M);

    m_color.a = getParentAbsoluteOpacity() * m_opacity;
    m_renderSystem->updateColorResource(m_colorId, m_color);

    m_label->validateAndClearFlags();
}

void CheckBox::draw(const RenderSystem& renderSystem) const {
    renderSystem.drawTexture(m_transformId, m_colorId, m_texCoordResourceId, m_M[3][2]);
    m_label->draw(renderSystem);
}

void CheckBox::updateTexCoordResource() {
    if (m_isChecked) {
        m_renderSystem->updateTexCoordResource(m_texCoordResourceId, glm::vec4(0.5f, 1.0f, 0.5f, 0.0f));
    } else {
        m_renderSystem->updateTexCoordResource(m_texCoordResourceId, glm::vec4(0.5f, 1.0f, 0.0f, 0.0f));
    }
}

void CheckBox::setState(State state) {
    if (m_state == state) {
        return;
    }

    m_state = state;

    glm::vec4 targetColor;

    if (m_state == State::Idle) {
        targetColor = glm::vec4(m_idleColor, m_opacity);
    } else if (m_state == State::Hover) {
        targetColor = glm::vec4(m_hoverColor, m_opacity);
    } else if (m_state == State::Pressed) {
        targetColor = glm::vec4(m_pressedColor, m_opacity);
    } else {
        targetColor = glm::vec4(m_disabledColor, m_opacity);
    }

    m_colorAnim->reset(glm::vec4(m_color.r, m_color.g, m_color.b, m_opacity), targetColor);

    if (!m_colorAnim->isActive()) {
        m_form->getAnimator()->add(m_colorAnim);
    }
}
} // namespace crisp::gui