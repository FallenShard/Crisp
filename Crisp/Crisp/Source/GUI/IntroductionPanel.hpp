#pragma once

#include "Panel.hpp"

namespace crisp
{
    class Application;
}

namespace crisp::gui
{
    class Form;
    class Label;
    class Button;

    class IntroductionPanel : public Panel
    {
    public:
        IntroductionPanel(Form* parentForm, Application* app);
        virtual ~IntroductionPanel();
    };
}