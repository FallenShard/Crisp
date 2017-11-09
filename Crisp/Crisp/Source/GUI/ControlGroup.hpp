#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Control.hpp"

namespace crisp::gui
{
    class ControlGroup : public Control
    {
    public:
        ControlGroup(Form* parentForm);
        virtual ~ControlGroup();

        void addControl(std::unique_ptr<Control> control);
        void removeControl(const std::string& id);

        virtual float getWidth() const;
        virtual float getHeight() const;

        virtual Rect<float> getInteractionBounds() const;
        virtual void onMouseMoved(float x, float y);
        virtual void onMouseEntered(float x, float y);
        virtual void onMouseExited(float x, float y);
        virtual void onMousePressed(float x, float y);
        virtual void onMouseReleased(float x, float y);

        virtual void setValidationFlags(ValidationFlags validationFlags) override;
        virtual bool needsValidation() override;
        virtual void validate() override;

        virtual void draw(const RenderSystem& renderSystem) const override;

        virtual void printDebugId() const override;
        virtual Control* getControlById(const std::string& id);
        virtual void visit(std::function<void(Control*)> func) override;

    protected:
        glm::vec2 m_prevMousePos;
        std::vector<std::unique_ptr<Control>> m_children;
    };
}