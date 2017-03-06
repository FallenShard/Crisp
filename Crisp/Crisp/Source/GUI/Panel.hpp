#pragma once

#include <functional>

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

            void setClickCallback(std::function<void()> callback);
            virtual void onMouseReleased(float x, float y) override;

        private:
            ColorPalette m_color;

            std::function<void()> m_clickCallback;
        };
    }
}