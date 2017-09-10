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
            Panel(Form* parentForm);
            ~Panel();

            virtual void validate() override;

            virtual void draw(RenderSystem& visitor) override;

            void setClickCallback(std::function<void()> callback);
            virtual void onMouseReleased(float x, float y) override;

        private:
            std::function<void()> m_clickCallback;

            unsigned int m_transformId;
            unsigned int m_colorId;
        };
    }
}