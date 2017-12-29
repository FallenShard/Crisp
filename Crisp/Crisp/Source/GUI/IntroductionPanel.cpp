#include "IntroductionPanel.hpp"

#include <sstream>
#include <iomanip>

#include "Core/Application.hpp"
#include "Core/SceneContainer.hpp"
#include "Form.hpp"
#include "Label.hpp"
#include "Button.hpp"
#include "Slider.hpp"
#include "ComboBox.hpp"
#include "Panel.hpp"
#include "DebugRect.hpp"
#include "DoubleSlider.hpp"

#include "VesperGui.hpp"

namespace crisp::gui
{
    IntroductionPanel::IntroductionPanel(Form* parentForm, Application* app)
        : Panel(parentForm)
    {
        setId("welcomePanel");
        setSizeHint({ 300, 500 });
        setPadding({ 20, 20 });
        setAnchor(Anchor::Center);
        setVerticalSizingPolicy(SizingPolicy::WrapContent);
        setHorizontalSizingPolicy(SizingPolicy::WrapContent);
        setOpacity(0.0f);

        auto welcomeLabel = std::make_unique<Label>(parentForm, "Welcome to Crisp!", 22);
        welcomeLabel->setPosition({ 0, 0 });
        welcomeLabel->setAnchor(Anchor::CenterTop);
        addControl(std::move(welcomeLabel));
        
        auto selectLabel = std::make_unique<Label>(parentForm, "Select your environment:");
        selectLabel->setPosition({ 0, 50 });
        selectLabel->setAnchor(Anchor::CenterTop);
        addControl(std::move(selectLabel));
        
        auto crispButton = std::make_unique<Button>(parentForm);
        crispButton->setId("crispButton");
        crispButton->setText("Crisp (Real-time Renderer)");
        crispButton->setFontSize(18);
        crispButton->setPosition({ 0, 80 });
        crispButton->setSizeHint({ 250, 50 });
        crispButton->setBackgroundColor({ 0.3f, 0.3f, 0.3f });
        crispButton->setTextColor(glm::vec3(1.0f), State::Idle);
        crispButton->setTextColor(glm::vec3(1.0f, 0.7f, 0.0f), State::Hover);
        crispButton->setTextColor(glm::vec3(0.8f, 0.4f, 0.0f), State::Pressed);
        crispButton->setAnchor(Anchor::CenterTop);
        crispButton->clicked += [app, parentForm, this]
        {
            auto sceneContainer = app->getSceneContainer();

            parentForm->remove("welcomePanel", 0.5f);
            Panel* statusBar = parentForm->getControlById<Panel>("statusBar");

            auto comboBox = std::make_unique<ComboBox>(parentForm);
            comboBox->setId("sceneComboBox");
            comboBox->setPosition({ 0, 0 });
            comboBox->setItems(SceneContainer::getSceneNames());
            comboBox->itemSelected.subscribe<SceneContainer, &SceneContainer::onSceneSelected>(sceneContainer);
            statusBar->addControl(std::move(comboBox));
        };
        addControl(std::move(crispButton));
        
        auto vesperButton = std::make_unique<Button>(parentForm);
        vesperButton->setId("vesperButton");
        vesperButton->setText("Vesper (Offline Ray Tracer)");
        vesperButton->setFontSize(18);
        vesperButton->setPosition({ 0, 150 });
        vesperButton->setSizeHint({ 250, 50 });
        vesperButton->setBackgroundColor({ 0.3f, 0.3f, 0.3f });
        vesperButton->setAnchor(Anchor::CenterTop);
        vesperButton->clicked += [parentForm, app]
        {
            auto sceneContainer = app->getSceneContainer();

            parentForm->remove("welcomePanel", 0.5f);
            sceneContainer->onSceneSelected("Ray Tracer");

            
        };
        addControl(std::move(vesperButton));

        auto quitButton = std::make_unique<Button>(parentForm);
        quitButton->setId("quitButton");
        quitButton->setText("Quit");
        quitButton->setFontSize(18);
        quitButton->setPosition({ 0, 220 });
        quitButton->setSizeHint({ 250, 50 });
        quitButton->setBackgroundColor({ 0.3f, 0.3f, 0.3f });
        quitButton->setAnchor(Anchor::CenterTop);
        quitButton->clicked += [app]
        {
            app->close();
        };
        addControl(std::move(quitButton));

        //auto slider = std::make_unique<Slider>(parentForm);
        //slider->setPosition({ 0, 300 });
        //slider->setMinValue(1);
        //slider->setMaxValue(7);
        //slider->setValue(1);
        //slider->setIncrement(4);
        //slider->setAnchor(Anchor::CenterTop);
        //addControl(std::move(slider));
        //
        //auto dslider = std::make_unique<DoubleSlider>(parentForm);
        //dslider->setPosition({ 0, 350 });
        //dslider->setMinValue(1);
        //dslider->setMaxValue(3);
        //
        //dslider->setAnchor(Anchor::CenterTop);
        //addControl(std::move(dslider));
    }

    IntroductionPanel::~IntroductionPanel()
    {
    }
}