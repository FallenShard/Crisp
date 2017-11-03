#pragma once

#include <functional>

#include "ControlGroup.hpp"
#include "DrawComponents/ColorRectDrawComponent.hpp"

namespace crisp
{
    namespace gui
    {
        class Panel : public ControlGroup
        {
        public:
            Panel(Form* parentForm);
            ~Panel();

            virtual void validate() override;

            virtual void draw(const RenderSystem& renderSystem) const override;

            void setClickCallback(std::function<void()> callback);
            virtual void onMouseReleased(float x, float y) override;

        private:
            std::function<void()> m_clickCallback;

            ColorRectDrawComponent m_drawComponent;
        };
    }
}