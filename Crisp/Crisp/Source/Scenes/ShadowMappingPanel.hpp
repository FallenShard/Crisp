#pragma once

#include "GUI/Panel.hpp"

namespace crisp
{
    class ShadowMappingScene;
}

namespace crisp::gui
{
    class Form;
    class Label;
    class Slider;
    class Button;

    class ShadowMappingPanel : public Panel
    {
    public:
        ShadowMappingPanel(Form* parentForm, ShadowMappingScene* scene);
        virtual ~ShadowMappingPanel();
    };
}