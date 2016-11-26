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
            ControlGroup();

            void addControl(std::shared_ptr<Control> control);
            void removeControl(const std::string& id);

            virtual void useAbsoluteSizing(bool absoluteSizing);
            virtual glm::vec2 getSize() const;

            virtual void onMouseMoved(float x, float y);
            virtual void onMouseEntered();
            virtual void onMouseExited();
            virtual void onMousePressed(float x, float y);
            virtual void onMouseReleased(float x, float y);

            virtual void invalidate() override;
            virtual bool needsValidation() override;
            virtual void validate() override;

            virtual void draw(RenderSystem& visitor) override;

            virtual Control* getControlById(const std::string& id);

        protected:
            glm::vec2 m_prevMousePos;
            std::vector<std::shared_ptr<Control>> m_children;

            bool m_useAbsoluteSizing;
        };
    }
}