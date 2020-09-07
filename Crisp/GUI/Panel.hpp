#pragma once

#include <functional>

#include "ControlGroup.hpp"
#include "DrawComponents/RectDrawComponent.hpp"

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

            void applyVerticalLayout(float spacing);

        private:
            std::function<void()> m_clickCallback;

            RectDrawComponent m_drawComponent;
        };
    }
}