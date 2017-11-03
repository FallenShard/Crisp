#pragma once

#include <CrispCore/Event.hpp>

#include "GUI/Control.hpp"
#include "GUI/DrawComponents/ColorRectDrawComponent.hpp"

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
        virtual void onMousePressed(float x, float y) override;
        virtual void onMouseReleased(float x, float y) override;

        virtual bool needsValidation() override;
        virtual void validate() override;

        virtual void draw(const RenderSystem& renderSystem) const override;

        Event<std::string> itemSelected;

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

        ColorRectDrawComponent m_drawComponent;

        std::unique_ptr<ControlGroup> m_itemsPanel;

        bool m_showPanel;
    };
}