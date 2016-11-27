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

            void setColor(ColorPalette color);
            ColorPalette getColor() const;

            virtual void draw(RenderSystem& visitor) override;

        private:
            ColorPalette m_color;
        };
    }
}