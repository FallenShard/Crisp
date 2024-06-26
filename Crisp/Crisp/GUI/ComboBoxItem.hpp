#pragma once

#include <Crisp/Core/Event.hpp>

#include <Crisp/Animation/PropertyAnimation.hpp>

#include <Crisp/Gui/Control.hpp>
#include <Crisp/Gui/DrawComponents/RectDrawComponent.hpp>

namespace crisp::gui {
class Label;

class ComboBoxItem : public Control {
public:
    ComboBoxItem(Form* parentForm, std::string = "Button");
    virtual ~ComboBoxItem();

    const std::string& getText() const;
    void setText(const std::string& text);
    void setTextColor(const glm::vec3& color);
    void setTextColor(const glm::vec3& color, State state);
    void setFontSize(unsigned int fontSize);

    void setEnabled(bool enabled);
    void setBackgroundColor(const glm::vec3& color);
    void setBackgroundColor(const glm::vec3& color, State state);

    virtual void onMouseEntered(float x, float y) override;
    virtual void onMouseExited(float x, float y) override;
    virtual bool onMousePressed(float x, float y) override;
    virtual bool onMouseReleased(float x, float y) override;

    virtual void validate() override;

    virtual void draw(const RenderSystem& renderSystem) const override;

    Event<> clicked;

private:
    void setState(State state);

    State m_state;

    struct Color {
        glm::vec3 background;
        glm::vec3 text;
    };

    std::vector<Color> m_stateColors;

    std::shared_ptr<PropertyAnimation<glm::vec4, Easing::SlowOut>> m_colorAnim;

    std::shared_ptr<PropertyAnimation<glm::vec4, Easing::SlowOut>> m_labelColorAnim;
    std::unique_ptr<Label> m_label;

    RectDrawComponent m_drawComponent;
};
} // namespace crisp::gui