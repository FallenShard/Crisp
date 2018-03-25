#include "ShadowMappingPanel.hpp"

#include <iostream>

#include "GUI/Slider.hpp"
#include "GUI/DoubleSlider.hpp"
#include "GUI/Button.hpp"

#include "ShadowMappingScene.hpp"

namespace crisp::gui
{
    ShadowMappingPanel::ShadowMappingPanel(Form* parentForm, ShadowMappingScene* scene)
        : Panel(parentForm)
    {
        setId("shadowMappingPanel");
        setPadding({ 20, 20 });
        setPosition({ 20, 40 });
        setVerticalSizingPolicy(SizingPolicy::WrapContent);
        setHorizontalSizingPolicy(SizingPolicy::WrapContent);

        int y = 0;

        auto numSamplesLabel = std::make_unique<Label>(parentForm, "Blur radius");
        numSamplesLabel->setPosition({ 0, y });
        addControl(std::move(numSamplesLabel));
        y += 20;

        auto blurRadiusSlider = std::make_unique<Slider>(parentForm);
        blurRadiusSlider->setId("blurRadiusSlider");
        blurRadiusSlider->setAnchor(Anchor::CenterTop);
        blurRadiusSlider->setPosition({ 0, y });
        blurRadiusSlider->setValues({ 0, 3, 7, 15, 25, 37 });
        blurRadiusSlider->setValue(2);
        blurRadiusSlider->valueChanged.subscribe<ShadowMappingScene, &ShadowMappingScene::setBlurRadius>(scene);
        addControl(std::move(blurRadiusSlider));
        y += 30;
    }

    ShadowMappingPanel::~ShadowMappingPanel()
    {
    }
}