#include "FluidSimulationPanel.hpp"

#include <iostream>

#include "GUI/Slider.hpp"
#include "GUI/DoubleSlider.hpp"
#include "GUI/Button.hpp"

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

        auto gxLabel = std::make_unique<Label>(parentForm, "Gravity X");
        gxLabel->setPosition({ 0, y });
        addControl(std::move(gxLabel));
        y += 20;

        auto gravityXSlider = std::make_unique<DoubleSlider>(parentForm);
        gravityXSlider->setId("gravityXSlider");
        gravityXSlider->setAnchor(Anchor::CenterTop);
        gravityXSlider->setPosition({ 0, y });
        gravityXSlider->setMinValue(-10.0);
        gravityXSlider->setMaxValue(+10.0);
        gravityXSlider->setIncrement(0.1);
        gravityXSlider->setPrecision(1);
        gravityXSlider->setValue(0.0);
        gravityXSlider->valueChanged += [sim](double value)
        {
            sim->setGravityX(static_cast<float>(value));
        };
        addControl(std::move(gravityXSlider));
        y += 30;

        auto gyLabel = std::make_unique<Label>(parentForm, "Gravity Y");
        gyLabel->setId("xyz");
        gyLabel->setPosition({ 0, y });
        addControl(std::move(gyLabel));
        y += 20;

        auto gravityYSlider = std::make_unique<DoubleSlider>(parentForm);
        gravityYSlider->setId("gravityYSlider");
        gravityYSlider->setAnchor(Anchor::CenterTop);
        gravityYSlider->setPosition({ 0, y });
        gravityYSlider->setMinValue(-10.0);
        gravityYSlider->setMaxValue(+10.0);
        gravityYSlider->setIncrement(0.1);
        gravityYSlider->setPrecision(1);
        gravityYSlider->setValue(-9.8);
        gravityYSlider->valueChanged += [sim](double value)
        {
            sim->setGravityY(static_cast<float>(value));
        };
        addControl(std::move(gravityYSlider));
        y += 30;

        auto gzLabel = std::make_unique<Label>(parentForm, "Gravity Z");
        gzLabel->setPosition({ 0, y });
        addControl(std::move(gzLabel));
        y += 20;

        auto gravityZSlider = std::make_unique<DoubleSlider>(parentForm);
        gravityZSlider->setId("gravityZSlider");
        gravityZSlider->setAnchor(Anchor::CenterTop);
        gravityZSlider->setPosition({ 0, y });
        gravityZSlider->setMinValue(-10.0);
        gravityZSlider->setMaxValue(+10.0);
        gravityZSlider->setIncrement(0.1);
        gravityZSlider->setPrecision(1);
        gravityZSlider->setValue(0.0);
        gravityZSlider->valueChanged += [sim](double value)
        {
            sim->setGravityZ(static_cast<float>(value));
        };
        addControl(std::move(gravityZSlider));
        y += 30;

        auto viscoLabel = std::make_unique<Label>(parentForm, "Viscosity");
        viscoLabel->setPosition({ 0, y });
        addControl(std::move(viscoLabel));
        y += 20;

        auto viscositySlider = std::make_unique<Slider>(parentForm);
        viscositySlider->setId("viscositySlider");
        viscositySlider->setAnchor(Anchor::CenterTop);
        viscositySlider->setPosition({ 0, y });
        viscositySlider->setMaxValue(30);
        viscositySlider->setMinValue(3);
        viscositySlider->setValue(3);
        viscositySlider->valueChanged += [sim](int value)
        {
            sim->setViscosity(static_cast<float>(value));
        };
        addControl(std::move(viscositySlider));
        y += 30;

        auto surfaceTensionLabel = std::make_unique<Label>(parentForm, "Surface Tension");
        surfaceTensionLabel->setPosition({ 0, y });
        addControl(std::move(surfaceTensionLabel));
        y += 20;

        auto surfaceTensionSlider = std::make_unique<Slider>(parentForm);
        surfaceTensionSlider->setId("surfaceTensionSlider");
        surfaceTensionSlider->setAnchor(Anchor::CenterTop);
        surfaceTensionSlider->setPosition({ 0, y });
        surfaceTensionSlider->setMaxValue(50);
        surfaceTensionSlider->setMinValue(1);
        surfaceTensionSlider->setValue(1);
        surfaceTensionSlider->valueChanged += [sim](int value)
        {
            sim->setSurfaceTension(static_cast<float>(value));
        };
        addControl(std::move(surfaceTensionSlider));
        y += 30;

        auto resetButton = std::make_unique<Button>(parentForm);
        resetButton->setId("resetButton");
        resetButton->setPosition({ 0, y });
        resetButton->setSizeHint({0, 30});
        resetButton->setText("Reset Simulation");
        resetButton->setHorizontalSizingPolicy(SizingPolicy::FillParent);
        resetButton->clicked += [sim]()
        {
            sim->reset();
        };
        addControl(std::move(resetButton));
    }

    FluidSimulationPanel::~FluidSimulationPanel()
    {
    }
}