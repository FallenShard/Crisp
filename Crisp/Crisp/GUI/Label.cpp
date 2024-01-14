#include <Crisp/Gui/Label.hpp>

#include <iostream>
#include <vector>

#include <Crisp/Gui/Form.hpp>

namespace crisp::gui {
Label::Label(Form* parentForm, const std::string& text, unsigned int fontSize)
    : Control(parentForm)
    , m_fontName("DinPro.ttf")
    , m_text(text)
    , m_drawComponent(parentForm->getRenderSystem(), m_text, m_fontName, fontSize) {
    m_M = glm::translate(glm::vec3(m_position, m_depthOffset));

    m_color = glm::vec4(1.0f);
    m_drawComponent.update(m_color);

    m_textExtent = m_drawComponent.update(m_text);
}

Label::~Label() {}

const std::string& Label::getText() const {
    return m_text;
}

void Label::setFontSize(unsigned int fontSize) {
    m_drawComponent.setFont(m_fontName, fontSize);
    m_textExtent = m_drawComponent.update(m_text);

    setValidationFlags(Validation::Geometry);
}

void Label::setText(const std::string& text) {
    m_text = text;
    m_textExtent = m_drawComponent.update(m_text);

    setValidationFlags(Validation::Geometry);
}

glm::vec2 Label::getTextExtent() const {
    return m_textExtent;
}

float Label::getWidth() const {
    return m_textExtent.x;
}

float Label::getHeight() const {
    return m_textExtent.y;
}

Rect<float> Label::getAbsoluteBounds() const {
    return {m_M[3][0], m_M[3][1], m_textExtent.x, m_textExtent.y};
}

void Label::validate() {
    auto absPos = getAbsolutePosition();
    auto absDepth = getAbsoluteDepth();

    glm::vec2 renderedPos(std::round(absPos.x), std::round(absPos.y + m_textExtent.y));
    m_M = glm::translate(glm::vec3(renderedPos, absDepth));
    m_drawComponent.update(m_M);

    m_color.a = getParentAbsoluteOpacity() * m_opacity;
    m_drawComponent.update(m_color);
}

void Label::draw(const RenderSystem& /*renderSystem*/) const {
    Rect<float> bounds = getAbsoluteBounds();
    bounds.y -= m_textExtent.y;
    // renderSystem.drawDebugRect(bounds, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    m_drawComponent.draw(m_M[3][2]);
}
} // namespace crisp::gui