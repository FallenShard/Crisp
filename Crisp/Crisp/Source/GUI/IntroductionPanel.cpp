#include "IntroductionPanel.hpp"

#include <sstream>
#include <iomanip>

#include "Core/Application.hpp"
#include "Form.hpp"
#include "Label.hpp"
#include "Button.hpp"
#include "Slider.hpp"
#include "ComboBox.hpp"
#include "Panel.hpp"
#include "DebugRect.hpp"

#include "VesperGui.hpp"
#include "FluidSimulationPanel.hpp"

namespace crisp::gui
{
    template <typename F>
    struct Holder
    {
        void add(F callback)
        {
            vec.push_back(callback);
        }

        std::vector<F> vec;
    };

    IntroductionPanel::IntroductionPanel(Form* parentForm, Application* app)
        : Panel(parentForm)
    {
        setId("welcomePanel");
        setSizeHint({ 300, 500 });
        setPadding({ 20, 20 });
        //setAnchor(Anchor::Center);
        //setVerticalSizingPolicy(SizingPolicy::WrapContent);
        //setHorizontalSizingPolicy(SizingPolicy::WrapContent);
        setOpacity(0.0f);

        //auto welcomeLabel = std::make_shared<Label>(parentForm, "Welcome to Crisp!", 22);
        //welcomeLabel->setPosition({ 0, 0 });
        //welcomeLabel->setAnchor(Anchor::CenterTop);
        //addControl(welcomeLabel);
        //
        //auto selectLabel = std::make_shared<Label>(parentForm, "Select your environment:");
        //selectLabel->setPosition({ 0, 50 });
        //selectLabel->setAnchor(Anchor::CenterTop);
        //addControl(selectLabel);
        //
        auto crispButton = std::make_unique<Button>(parentForm);
        crispButton->setId("crispButton");
        crispButton->setText("Crisp (Real-time Renderer)");
        crispButton->setFontSize(18);
        crispButton->setPosition({ 0, 0 });
        crispButton->setSizeHint({ 250, 50 });
        crispButton->setBackgroundColor({ 0.3f, 0.3f, 0.3f });
        crispButton->setTextColor(glm::vec3(1.0f), State::Idle);
        crispButton->setTextColor(glm::vec3(1.0f, 0.7f, 0.0f), State::Hover);
        crispButton->setTextColor(glm::vec3(0.8f, 0.4f, 0.0f), State::Pressed);
        crispButton->setAnchor(Anchor::CenterTop);
        crispButton->clicked += [app, parentForm, this]
        {
            static bool add = true;
            
            if (add)
            {
                //auto box = std::make_unique<Label>(parentForm);
                //box->setId("EXAMPLE");
                //box->setSizeHint({ 50.0f, 50.0f });
                //box->setColor({ 0.3f, 0.0f, 1.0f, 1.0f });
                //box->setPosition({ 150, 150 + 60 });
                //addControl(std::move(box));
                gui::VesperGui vesperGui;
                parentForm->add(vesperGui.buildSceneOptions(parentForm));
                //parentForm->add(vesperGui.buildProgressBar(parentForm));
            }
            else
            {
                parentForm->remove("vesperOptionsPanel");
                //removeControl("vesperOptionsPanel");
            }
            parentForm->printGuiTree();
            add = !add;
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
        vesperButton->clicked += [parentForm]
        {
            parentForm->visit([](Control* ctrl)
            {
                std::cout << ctrl->getId() << ": " << ctrl->getModelMatrix()[3][2] << std::endl;
            });
        };
        addControl(std::move(vesperButton));

        //gui::VesperGui vesperGui;
        //parentForm->add(vesperGui.buildSceneOptions(parentForm));
        //
        //auto quitButton = std::make_shared<Button>(parentForm);
        //quitButton->setId("quitButton");
        //quitButton->setText("Quit");
        //quitButton->setFontSize(18);
        //quitButton->setPosition({ 0, 220 });
        //quitButton->setSizeHint({ 250, 50 });
        //quitButton->setBackgroundColor({ 0.3f, 0.3f, 0.3f });
        //quitButton->setAnchor(Anchor::CenterTop);
        //addControl(quitButton);
        //
        //crispButton->clicked += [app, parentForm]
        //{
        //    parentForm->remove("welcomePanel", 0.5f);
        //    parentForm->postGuiUpdate([app, parentForm]
        //    {
        //        Panel* statusBar = parentForm->getControlById<Panel>("statusBar");
        //
        //        auto comboBox = std::make_shared<ComboBox>(parentForm);
        //        comboBox->setId("sceneComboBox");
        //        comboBox->setPosition({ 0, 0 });
        //        statusBar->addControl(comboBox);
        //
        //        app->createScene();
        //    });
        //};
        //vesperButton->clicked += [app, parentForm]
        //{
        //    parentForm->postGuiUpdate([app, parentForm]
        //    {
        //        gui::VesperGui vesperGui;
        //        parentForm->add(vesperGui.buildSceneOptions(parentForm));
        //        parentForm->add(vesperGui.buildProgressBar(parentForm));
        //
        //        vesperGui.setupInputCallbacks(parentForm, app);
        //    });
        //
        //    parentForm->remove("welcomePanel", 0.5f);
        //};
        //quitButton->clicked += [app]
        //{
        //    app->quit();
        //};
        //
        //auto slider = std::make_shared<Slider>(parentForm);
        //slider->setAnchor(Anchor::CenterTop);
        //slider->setPosition({ 0, 300 });
        //addControl(slider);

        //auto comboBox = std::make_shared<ComboBox>(parentForm);
        //comboBox->setAnchor(Anchor::CenterTop);
        //comboBox->setPosition({ 0, 350 });
        //addControl(comboBox);
    }

    IntroductionPanel::~IntroductionPanel()
    {
    }
}