#include "FluidSimulationPanel.hpp"

#include <iostream>

#include "Slider.hpp"
#include "Button.hpp"

#include "Models/FluidSimulation.hpp"

namespace crisp::gui
{
    FluidSimulationPanel::FluidSimulationPanel(Form* parentForm, FluidSimulation* sim)
        : Panel(parentForm)
    {
        setId("fluidSimulationPanel");
        setPadding({ 20, 20 });
        setPosition({ 20, 40 });
        setVerticalSizingPolicy(SizingPolicy::WrapContent);
        setHorizontalSizingPolicy(SizingPolicy::WrapContent);

        int y = 0;

        auto panelLabel = std::make_shared<Label>(parentForm, "Fluid Simulation");
        panelLabel->setFontSize(18);
        panelLabel->setPosition({ 0, y });
        addControl(panelLabel);
        y += 30;

        auto gxLabel = std::make_shared<Label>(parentForm, "Gravity X");
        gxLabel->setPosition({ 0, y });
        addControl(gxLabel);
        y += 20;

        auto gravityXSlider = std::make_shared<Slider>(parentForm);
        gravityXSlider->setId("gravityXSlider");
        gravityXSlider->setAnchor(Anchor::CenterTop);
        gravityXSlider->setPosition({ 0, y });
        gravityXSlider->setValue(50);
        addControl(gravityXSlider);
        y += 30;

        auto gyLabel = std::make_shared<Label>(parentForm, "Gravity Y");
        gyLabel->setPosition({ 0, y });
        addControl(gyLabel);
        y += 20;

        auto gravityYSlider = std::make_shared<Slider>(parentForm);
        gravityYSlider->setId("gravityYSlider");
        gravityYSlider->setAnchor(Anchor::CenterTop);
        gravityYSlider->setPosition({ 0, y });
        gravityYSlider->setValue(99);
        addControl(gravityYSlider);
        y += 30;

        auto gzLabel = std::make_shared<Label>(parentForm, "Gravity Z");
        gzLabel->setPosition({ 0, y });
        addControl(gzLabel);
        y += 20;

        auto gravityZSlider = std::make_shared<Slider>(parentForm);
        gravityZSlider->setId("gravityZSlider");
        gravityZSlider->setAnchor(Anchor::CenterTop);
        gravityZSlider->setPosition({ 0, y });
        gravityZSlider->setValue(50);
        addControl(gravityZSlider);
        y += 30;

        gravityXSlider->valueChanged.subscribe([sim](int sliderPos)
        {
            float value = static_cast<float>(sliderPos) / 5.0f - 10.0f;
            sim->setGravityX(value);
        });

        gravityYSlider->valueChanged.subscribe([sim](int sliderPos)
        {
            float value = static_cast<float>(sliderPos) / 5.0f - 10.0f;
            sim->setGravityY(value);
        });

        gravityZSlider->valueChanged.subscribe([sim](int sliderPos)
        {
            float value = static_cast<float>(sliderPos) / 5.0f - 10.0f;;
            sim->setGravityZ(value);
        });

        auto viscoLabel = std::make_shared<Label>(parentForm, "Viscosity");
        viscoLabel->setPosition({ 0, y });
        addControl(viscoLabel);
        y += 20;

        auto viscositySlider = std::make_shared<Slider>(parentForm);
        viscositySlider->setId("viscositySlider");
        viscositySlider->setAnchor(Anchor::CenterTop);
        viscositySlider->setPosition({ 0, y });
        viscositySlider->setValue(3);
        addControl(viscositySlider);
        y += 30;

        viscositySlider->valueChanged.subscribe([sim](int sliderPos)
        {
            float viscosity = static_cast<float>(sliderPos);
            sim->setViscosity(viscosity);
        });

        auto resetButton = std::make_shared<Button>(parentForm);
        resetButton->setId("resetButton");
        resetButton->setPosition({ 0, y });
        resetButton->setSizeHint({0, 30});
        resetButton->setText("Reset Simulation");
        resetButton->setHorizontalSizingPolicy(SizingPolicy::FillParent);
        addControl(resetButton);

        resetButton->clicked.subscribe([sim]()
        {
            sim->reset();
        });
    }

    FluidSimulationPanel::~FluidSimulationPanel()
    {
    }
}