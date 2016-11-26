#pragma once

#include "ControlGroup.hpp"

namespace crisp
{
    namespace gui
    {
        class Panel : public ControlGroup
        {
        public:
            Panel(RenderSystem* renderSystem);
            ~Panel();

            virtual void validate() override;

            ColorPalette getColor() const;

            virtual void draw(RenderSystem& visitor) override;
        };
    }
}