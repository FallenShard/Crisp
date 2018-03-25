#include "AmbientOcclusionPanel.hpp"

#include <iostream>

#include "GUI/Slider.hpp"
#include "GUI/DoubleSlider.hpp"
#include "GUI/Button.hpp"

#include "AmbientOcclusionScene.hpp"

namespace crisp::gui
{
    AmbientOcclusionPanel::AmbientOcclusionPanel(Form* parentForm, AmbientOcclusionScene* scene)
        : Panel(parentForm)
    {
        setId("ambientOcclusionPanel");
        setPadding({ 20, 20 });
        setPosition({ 20, 40 });
        setVerticalSizingPolicy(SizingPolicy::WrapContent);
        setHorizontalSizingPolicy(SizingPolicy::WrapContent);

        int y = 0;

        auto numSamplesLabel = std::make_unique<Label>(parentForm, "Number of samples");
        numSamplesLabel->setPosition({ 0, y });
        addControl(std::move(numSamplesLabel));
        y += 20;

        auto numSamplesSlider = std::make_unique<Slider>(parentForm);
        numSamplesSlider->setId("numSamplesSlider");
        numSamplesSlider->setAnchor(Anchor::CenterTop);
        numSamplesSlider->setPosition({ 0, y });
        numSamplesSlider->setValues({ 16, 32, 64, 128, 256, 512 });
        numSamplesSlider->setValue(3);
        numSamplesSlider->valueChanged.subscribe<AmbientOcclusionScene, &AmbientOcclusionScene::setNumSamples>(scene);
        addControl(std::move(numSamplesSlider));
        y += 30;

        auto radiusLabel = std::make_unique<Label>(parentForm, "Radius");
        radiusLabel->setPosition({ 0, y });
        addControl(std::move(radiusLabel));
        y += 20;

        auto radiusSlider = std::make_unique<DoubleSlider>(parentForm);
        radiusSlider->setId("radiusSlider");
        radiusSlider->setAnchor(Anchor::CenterTop);
        radiusSlider->setPosition({ 0, y });
        radiusSlider->setMinValue(0.1);
        radiusSlider->setMaxValue(2.0);
        radiusSlider->setIncrement(0.1);
        radiusSlider->setPrecision(1);
        radiusSlider->setValue(0.5);
        radiusSlider->valueChanged.subscribe<AmbientOcclusionScene, &AmbientOcclusionScene::setRadius>(scene);
        addControl(std::move(radiusSlider));
        y += 30;
    }

    AmbientOcclusionPanel::~AmbientOcclusionPanel()
    {
    }
}