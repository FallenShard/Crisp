#pragma once

#include "GUI/Panel.hpp"

namespace crisp
{
    class AmbientOcclusionScene;
}

namespace crisp::gui
{
    class Form;
    class Label;
    class Slider;
    class Button;

    class AmbientOcclusionPanel : public Panel
    {
    public:
        AmbientOcclusionPanel(Form* parentForm, AmbientOcclusionScene* scene);
        virtual ~AmbientOcclusionPanel();
    };
}