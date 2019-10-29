#include "ShadowMappingPanel.hpp"

#include <iostream>

#include "GUI/Slider.hpp"
#include "GUI/DoubleSlider.hpp"
#include "GUI/Button.hpp"
#include "GUI/ComboBox.hpp"

#include "ShadowMappingScene.hpp"

namespace crisp::gui
{
    std::unique_ptr<Panel> createShadowMappingSceneGui(Form* form, ShadowMappingScene* scene)
    {
        std::unique_ptr<Panel> panel = std::make_unique<Panel>(form);

        panel->setId("shadowMappingPanel");
        panel->setPadding({ 20, 20 });
        panel->setPosition({ 20, 40 });
        panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);
        panel->setHorizontalSizingPolicy(SizingPolicy::WrapContent);

        int y = 0;

        auto numSamplesLabel = std::make_unique<Label>(form, "CSM Split Lambda");
        numSamplesLabel->setPosition({ 0, y });
        panel->addControl(std::move(numSamplesLabel));
        y += 20;

        auto lambdaSlider = std::make_unique<DoubleSlider>(form);
        lambdaSlider->setId("lambdaSlider");
        lambdaSlider->setAnchor(Anchor::TopCenter);
        lambdaSlider->setOrigin(Origin::TopCenter);
        lambdaSlider->setPosition({ 0, y });
        lambdaSlider->setMinValue(0.0f);
        lambdaSlider->setMaxValue(1.0f);
        lambdaSlider->setValue(0.5f);
        lambdaSlider->setIncrement(0.01f);
        lambdaSlider->valueChanged.subscribe<&ShadowMappingScene::setSplitLambda>(scene);
        panel->addControl(std::move(lambdaSlider));
        y += 30;

        auto roughnessSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
        roughnessSlider->setId("roughnessSlider");
        roughnessSlider->setAnchor(Anchor::TopCenter);
        roughnessSlider->setOrigin(Origin::TopCenter);
        roughnessSlider->setPosition({ 0, y });
        roughnessSlider->setValue(0.0f);
        roughnessSlider->setIncrement(0.01f);
        roughnessSlider->valueChanged.subscribe<&ShadowMappingScene::setRoughness>(scene);
        panel->addControl(std::move(roughnessSlider));
        y += 30;

        auto metallicSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
        metallicSlider->setId("metallicSlider");
        metallicSlider->setAnchor(Anchor::TopCenter);
        metallicSlider->setOrigin(Origin::TopCenter);
        metallicSlider->setPosition({ 0, y });
        metallicSlider->setValue(0.0f);
        metallicSlider->setIncrement(0.01f);
        metallicSlider->valueChanged.subscribe<&ShadowMappingScene::setMetallic>(scene);
        panel->addControl(std::move(metallicSlider));
        y += 30;

        auto redSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
        redSlider->setId("redSlider");
        redSlider->setAnchor(Anchor::TopCenter);
        redSlider->setOrigin(Origin::TopCenter);
        redSlider->setPosition({ 0, y });
        redSlider->setValue(0.0f);
        redSlider->setIncrement(0.01f);
        redSlider->valueChanged.subscribe<&ShadowMappingScene::setRedAlbedo>(scene);
        panel->addControl(std::move(redSlider));
        y += 30;

        auto greenSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
        greenSlider->setId("greenSlider");
        greenSlider->setAnchor(Anchor::TopCenter);
        greenSlider->setOrigin(Origin::TopCenter);
        greenSlider->setPosition({ 0, y });
        greenSlider->setValue(0.0f);
        greenSlider->setIncrement(0.01f);
        greenSlider->valueChanged.subscribe<&ShadowMappingScene::setGreenAlbedo>(scene);
        panel->addControl(std::move(greenSlider));
        y += 30;

        auto blueSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
        blueSlider->setId("blueSlider");
        blueSlider->setAnchor(Anchor::TopCenter);
        blueSlider->setOrigin(Origin::TopCenter);
        blueSlider->setPosition({ 0, y });
        blueSlider->setValue(0.0f);
        blueSlider->setIncrement(0.01f);
        blueSlider->valueChanged.subscribe<&ShadowMappingScene::setBlueAlbedo>(scene);
        panel->addControl(std::move(blueSlider));
        y += 30;

        std::vector<std::string> materials =
        {
            "RustedIron",
            "Limestone",
            "MetalGrid",
            "MixedMoss",
            "RedBricks",
            "GreenCeramic"
        };

        auto comboBox = std::make_unique<gui::ComboBox>(form);
        comboBox->setId("sceneComboBox");
        comboBox->setPosition({ 0, y });
        comboBox->setItems(materials);
        comboBox->itemSelected.subscribe<&ShadowMappingScene::onMaterialSelected>(scene);
        panel->addControl(std::move(comboBox));

        return panel;
    }
}