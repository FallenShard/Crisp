#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Control.hpp"

namespace crisp
{
    namespace gui
    {
        class ControlGroup : public Control
        {
        public:
            ControlGroup(Form* parentForm);

            void addControl(std::shared_ptr<Control> control);
            void removeControl(const std::string& id);

            virtual float getWidth() const;
            virtual float getHeight() const;

            virtual void onMouseMoved(float x, float y);
            virtual void onMouseEntered();
            virtual void onMouseExited();
            virtual void onMousePressed(float x, float y);
            virtual void onMouseReleased(float x, float y);

            virtual void setValidationFlags(Validation validation) override;
            virtual bool needsValidation() override;
            virtual void validate() override;

            virtual void draw(RenderSystem& renderSystem) override;

            virtual Control* getControlById(const std::string& id);

        protected:
            glm::vec2 m_prevMousePos;
            std::vector<std::shared_ptr<Control>> m_children;
            glm::vec2 m_cachedChildrenSize;
        };
    }
}