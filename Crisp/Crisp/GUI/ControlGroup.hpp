#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Control.hpp"

namespace crisp::gui
{
enum class LayoutType
{
    Vertical,
    Horizontal,
    Absolute
};

class ControlGroup : public Control
{
public:
    ControlGroup(Form* parentForm);
    virtual ~ControlGroup();

    void addControl(std::unique_ptr<Control> control);
    void addControlDirect(std::unique_ptr<Control> control);
    void removeControl(const std::string& id);
    void clearControls();

    virtual float getWidth() const;
    virtual float getHeight() const;

    virtual Rect<float> getInteractionBounds() const;
    virtual void onMouseMoved(float x, float y);
    virtual void onMouseEntered(float x, float y);
    virtual void onMouseExited(float x, float y);
    virtual bool onMousePressed(float x, float y);
    virtual bool onMouseReleased(float x, float y);

    virtual void validate() override;

    virtual void draw(const RenderSystem& renderSystem) const override;

    virtual void printDebugId() const override;
    virtual Control* getControlById(const std::string& id);
    virtual void visit(std::function<void(Control*)> func) override;

    void setLayoutType(LayoutType layoutType)
    {
        m_layoutType = layoutType;
    }

    void setLayoutSpacing(float spacing)
    {
        m_spacing = spacing;
    }

protected:
    glm::vec2 m_prevMousePos;
    std::vector<std::unique_ptr<Control>> m_children;

    LayoutType m_layoutType{LayoutType::Absolute};
    float m_spacing{10.0f};
};
} // namespace crisp::gui
