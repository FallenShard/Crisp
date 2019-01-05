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

        auto numSamplesLabel = std::make_unique<Label>(parentForm, "CSM Split Lambda");
        numSamplesLabel->setPosition({ 0, y });
        addControl(std::move(numSamplesLabel));
        y += 20;

        //auto blurRadiusSlider = std::make_unique<Slider>(parentForm);
        //blurRadiusSlider->setId("lambdaSlider");
        //blurRadiusSlider->setAnchor(Anchor::CenterTop);
        //blurRadiusSlider->setPosition({ 0, y });
        //blurRadiusSlider->setValues({ 0, 3, 7, 15, 25, 37 });
        //blurRadiusSlider->setValue(2);
        //blurRadiusSlider->valueChanged.subscribe<&ShadowMappingScene::setSplitLambda>(scene);
        //addControl(std::move(blurRadiusSlider));
        auto lambdaSlider = std::make_unique<DoubleSlider>(parentForm);
        lambdaSlider->setId("lambdaSlider");
        lambdaSlider->setAnchor(Anchor::CenterTop);
        lambdaSlider->setPosition({ 0, y });
        lambdaSlider->setMinValue(0.0f);
        lambdaSlider->setMaxValue(1.0f);
        lambdaSlider->setValue(0.5f);
        lambdaSlider->setIncrement(0.01f);
        lambdaSlider->valueChanged.subscribe<&ShadowMappingScene::setSplitLambda>(scene);
        addControl(std::move(lambdaSlider));
        y += 30;

        auto roughnessSlider = std::make_unique<DoubleSlider>(parentForm);
        roughnessSlider->setId("roughnessSlider");
        roughnessSlider->setAnchor(Anchor::CenterTop);
        roughnessSlider->setPosition({ 0, y });
        roughnessSlider->setMinValue(0.0f);
        roughnessSlider->setMaxValue(1.0f);
        roughnessSlider->setValue(0.1f);
        roughnessSlider->setIncrement(0.01f);
        roughnessSlider->valueChanged.subscribe<&ShadowMappingScene::setRoughness>(scene);
        addControl(std::move(roughnessSlider));
        y += 30;

        auto metallicSlider = std::make_unique<DoubleSlider>(parentForm);
        metallicSlider->setId("metallicSlider");
        metallicSlider->setAnchor(Anchor::CenterTop);
        metallicSlider->setPosition({ 0, y });
        metallicSlider->setMinValue(0.0f);
        metallicSlider->setMaxValue(1.0f);
        metallicSlider->setValue(0.0f);
        metallicSlider->setIncrement(0.01f);
        metallicSlider->valueChanged.subscribe<&ShadowMappingScene::setMetallic>(scene);
        addControl(std::move(metallicSlider));
        y += 30;
    }

    ShadowMappingPanel::~ShadowMappingPanel()
    {
    }
}