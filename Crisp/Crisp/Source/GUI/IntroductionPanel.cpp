#include "IntroductionPanel.hpp"

#include <sstream>
#include <iomanip>

#include "Core/Application.hpp"
#include "Form.hpp"
#include "Label.hpp"
#include "Button.hpp"
#include "Slider.hpp"

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

        auto welcomeLabel = std::make_shared<Label>(parentForm, "Welcome to Crisp!", 22);
        welcomeLabel->setPosition({ 0, 0 });
        welcomeLabel->setAnchor(Anchor::CenterTop);
        addControl(welcomeLabel);

        auto selectLabel = std::make_shared<Label>(parentForm, "Select your environment:");
        selectLabel->setPosition({ 0, 50 });
        selectLabel->setAnchor(Anchor::CenterTop);
        addControl(selectLabel);

        auto crispButton = std::make_shared<Button>(parentForm);
        crispButton->setId("crispButton");
        crispButton->setText("Crisp (Real-time Renderer)");
        crispButton->setFontSize(18);
        crispButton->setPosition({ 0, 80 });
        crispButton->setSizeHint({ 250, 50 });
        crispButton->setIdleColor({ 0.3f, 0.5f, 0.3f });
        crispButton->setPressedColor({ 0.2f, 0.3f, 0.2f });
        crispButton->setHoverColor({ 0.4f, 0.7f, 0.4f });
        crispButton->setAnchor(Anchor::CenterTop);
        addControl(crispButton);

        auto vesperButton = std::make_shared<Button>(parentForm);
        vesperButton->setId("vesperButton");
        vesperButton->setText("Vesper (Offline Ray Tracer)");
        vesperButton->setFontSize(18);
        vesperButton->setPosition({ 0, 150 });
        vesperButton->setSizeHint({ 250, 50 });
        vesperButton->setIdleColor({ 0.3f, 0.3f, 0.5f });
        vesperButton->setPressedColor({ 0.2f, 0.2f, 0.3f });
        vesperButton->setHoverColor({ 0.4f, 0.4f, 0.7f });
        vesperButton->setAnchor(Anchor::CenterTop);
        addControl(vesperButton);

        auto quitButton = std::make_shared<Button>(parentForm);
        quitButton->setId("quitButton");
        quitButton->setText("Quit");
        quitButton->setFontSize(18);
        quitButton->setPosition({ 0, 220 });
        quitButton->setSizeHint({ 250, 50 });
        quitButton->setIdleColor({ 0.5f, 0.3f, 0.3f });
        quitButton->setPressedColor({ 0.3f, 0.2f, 0.2f });
        quitButton->setHoverColor({ 0.7f, 0.4f, 0.4f });
        quitButton->setAnchor(Anchor::CenterTop);
        addControl(quitButton);

        crispButton->clicked.subscribe([app, parentForm]()
        {
            app->createScene();
            parentForm->remove("welcomePanel", 0.5f);
        });
        vesperButton->clicked.subscribe([app, parentForm]()
        {
            parentForm->postGuiUpdate([app, parentForm]()
            {
                gui::VesperGui vesperGui;
                parentForm->add(vesperGui.buildSceneOptions(parentForm));
                parentForm->add(vesperGui.buildProgressBar(parentForm));

                vesperGui.setupInputCallbacks(parentForm, app);
            });

            parentForm->remove("welcomePanel", 0.5f);
        });
        quitButton->clicked.subscribe([app]()
        {
            app->quit();
        });

        auto slider = std::make_shared<Slider>(parentForm);
        slider->setAnchor(Anchor::CenterTop);
        slider->setPosition({ 0, 300 });
        addControl(slider);
    }

    IntroductionPanel::~IntroductionPanel()
    {
    }
}