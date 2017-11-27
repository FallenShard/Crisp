#pragma once

#include "GUI/Panel.hpp"

namespace crisp
{
    class FluidSimulation;
}

namespace crisp::gui
{
    class Form;
    class Label;
    class Slider;
    class Button;

    class FluidSimulationPanel : public Panel
    {
    public:
        FluidSimulationPanel(Form* parentForm, FluidSimulation* fluidSimulation);
        virtual ~FluidSimulationPanel();
    };
}