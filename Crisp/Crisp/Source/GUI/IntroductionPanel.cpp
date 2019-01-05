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

#include "Core/Window.hpp"

namespace crisp::gui
{
    std::unique_ptr<Panel> createIntroPanel(Form* form, Application* application)
    {
        std::unique_ptr<Panel> introPanel = std::make_unique<Panel>(form);

        introPanel->setId("introPanel");
        introPanel->setPosition({ 0, 0 });
        introPanel->setSizeHint({ 100, 100 });
        introPanel->setAnchor(Anchor::Center);
        introPanel->setVerticalSizingPolicy(SizingPolicy::WrapContent);
        introPanel->setHorizontalSizingPolicy(SizingPolicy::WrapContent);
        introPanel->setPadding({ 20, 20 });

        auto welcomeLabel = std::make_unique<Label>(form, "Welcome to Crisp!", 22);
        welcomeLabel->setPosition({ 0, 0 });
        welcomeLabel->setAnchor(Anchor::CenterTop);
        introPanel->addControl(std::move(welcomeLabel));

        auto selectLabel = std::make_unique<Label>(form, "Select your environment:");
        selectLabel->setPosition({ 0, 50 });
        selectLabel->setAnchor(Anchor::CenterTop);
        introPanel->addControl(std::move(selectLabel));

        auto crispButton = std::make_unique<Button>(form);
        crispButton->setId("crispButton");
        crispButton->setText("Crisp (Real-time Renderer)");
        crispButton->setFontSize(16);
        crispButton->setPosition({ 0, 80 });
        crispButton->setSizeHint({ 250, 50 });
        crispButton->setAnchor(Anchor::CenterTop);
        crispButton->clicked += [application, form, button = crispButton.get()]
        {
            button->setEnabled(false);
            auto sceneContainer = application->getSceneContainer();

            form->remove("introPanel", 0.5f);
            Panel* statusBar = form->getControlById<Panel>("statusBar");

            auto comboBox = std::make_unique<ComboBox>(form);
            comboBox->setId("sceneComboBox");
            comboBox->setPosition({ 0, 0 });
            comboBox->setItems(SceneContainer::getSceneNames());
            comboBox->itemSelected.subscribe<&SceneContainer::onSceneSelected>(sceneContainer);
            comboBox->itemSelected(SceneContainer::getDefaultScene());
            statusBar->addControl(std::move(comboBox));
        };
        introPanel->addControl(std::move(crispButton));

        auto vesperButton = std::make_unique<Button>(form);
        vesperButton->setId("vesperButton");
        vesperButton->setText("Vesper (Offline Ray Tracer)");
        vesperButton->setFontSize(16);
        vesperButton->setPosition({ 0, 150 });
        vesperButton->setSizeHint({ 250, 50 });
        vesperButton->setAnchor(Anchor::CenterTop);
        vesperButton->clicked += [form, application, button = vesperButton.get()]
        {
            button->setEnabled(false);
            auto sceneContainer = application->getSceneContainer();

            form->remove("introPanel", 0.5f);
            sceneContainer->onSceneSelected("Ray Tracer");
        };
        introPanel->addControl(std::move(vesperButton));

        auto quitButton = std::make_unique<Button>(form);
        quitButton->setId("quitButton");
        quitButton->setText("Quit");
        quitButton->setFontSize(18);
        quitButton->setPosition({ 0, 220 });
        quitButton->setSizeHint({ 250, 50 });
        quitButton->setAnchor(Anchor::CenterTop);
        quitButton->clicked += [application]
        {
            application->close();
        };
        introPanel->addControl(std::move(quitButton));

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
        //
        //auto button = std::make_unique<Button>(form, "Delete me!");
        //button->setAnchor(Anchor::CenterTop);
        //button->setPosition({ 0, 150 });
        //button->clicked += [p = introPanel.get(), b = button.get(), form]
        //{
        //    b->setEnabled(false);
        //    auto colorAnim = std::make_shared<PropertyAnimation<float, Easing::Linear>>(0.5f, 1.0f, 0.0f, [b](const auto& t)
        //    {
        //        b->setOpacity(t);
        //    });
        //    colorAnim->finished += [p, b]
        //    {
        //        p->removeControl(b->getId());
        //    };
        //    form->getAnimator()->add(colorAnim);
        //};
        //introPanel->addControl(std::move(button));

        return introPanel;
    }
}