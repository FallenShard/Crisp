#pragma once

#include <CrispCore/Event.hpp>

#include <Crisp/GUI/Control.hpp>
#include <Crisp/GUI/DrawComponents/RectDrawComponent.hpp>

namespace crisp::gui
{
class Label;
class ControlGroup;

class ComboBox : public Control
{
public:
    ComboBox(Form* parentForm, std::vector<std::string> elements = {});
    virtual ~ComboBox();

    virtual Rect<float> getInteractionBounds() const override;
    virtual void onMouseEntered(float x, float y) override;
    virtual void onMouseMoved(float x, float y) override;
    virtual void onMouseExited(float x, float y) override;
    virtual bool onMousePressed(float x, float y) override;
    virtual bool onMouseReleased(float x, float y) override;

    virtual void validate() override;

    virtual void draw(const RenderSystem& renderSystem) const override;

    Event<const std::string&> itemSelected;

    void setItems(const std::vector<std::string>& items);
    void selectItem(std::size_t index);

private:
    void setState(State state);

    State m_state;

    struct Color
    {
        glm::vec3 background;
        glm::vec3 text;
    };

    std::vector<Color> m_stateColors;

    std::unique_ptr<Label> m_label;

    RectDrawComponent m_drawComponent;

    std::unique_ptr<ControlGroup> m_itemsPanel;

    bool m_showPanel;
};
} // namespace crisp::gui