#include "VesperGui.hpp"

#include "Application.hpp"
#include "Control.hpp"
#include "Panel.hpp"
#include "Button.hpp"
#include "Form.hpp"

namespace crisp
{
    namespace gui
    {
        void VesperGui::setupInputCallbacks(Form* form, Application* app)
        {
            form->getControlById<gui::Button>("openButton")->setClickCallback([app]()
            {
                app->openSceneFile();
            });
            
            form->getControlById<gui::Button>("renderButton")->setClickCallback([app]()
            {
                app->startRayTracing();
            });
            
            form->getControlById<gui::Button>("stopButton")->setClickCallback([app]()
            {
                app->stopRayTracing();
            });
            
            form->getControlById<gui::Button>("saveButton")->setClickCallback([app]()
            {
                app->writeImageToExr();
            });

            auto progressLabel = form->getControlById<Label>("progressLabel");
            auto progressBar   = form->getControlById<Panel>("progressBar");

            app->rayTracerProgressed.subscribe([form, progressLabel, progressBar](float percentage, float timeSpent)
            {
                float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpent / 8.0f;
                
                std::stringstream stringStream;
                stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100 << " %    ETA: " << remainingPct << " s";
                progressLabel->setText(stringStream.str());
                progressBar->setWidthSizingBehavior(Sizing::FillParent, percentage);
            });
        }

        std::shared_ptr<Control> VesperGui::buildSceneOptions(Form* form)
        {
            auto panel = std::make_shared<gui::Panel>(form);
            panel->setId("vesperOptionsPanel");
            panel->setPosition({ 10, 30 });
            panel->setSize({ 200, 500 });
            panel->setPadding({ 10, 10 });
            panel->setAnchor(Anchor::TopLeft);
            panel->setHeightSizingBehavior(Sizing::WrapContent);

            auto button = std::make_shared<gui::Button>(form);
            button->setId("openButton");
            button->setText("Open XML Scene");
            button->setSize({ 100, 30 });
            button->setWidthSizingBehavior(Sizing::FillParent);
            panel->addControl(button);

            button = std::make_shared<gui::Button>(form);
            button->setId("renderButton");
            button->setText("Start Raytracing");
            button->setPosition({ 0, 40 });
            button->setSize({ 100, 30 });
            button->setWidthSizingBehavior(Sizing::FillParent);
            panel->addControl(button);

            button = std::make_shared<gui::Button>(form);
            button->setId("stopButton");
            button->setText("Stop Raytracing");
            button->setPosition({ 0, 80 });
            button->setSize({ 100, 30 });
            button->setWidthSizingBehavior(Sizing::FillParent);
            panel->addControl(button);

            button = std::make_shared<gui::Button>(form);
            button->setId("saveButton");
            button->setText("Save as EXR");
            button->setPosition({ 0, 120 });
            button->setSize({ 100, 30 });
            button->setWidthSizingBehavior(Sizing::FillParent);
            panel->addControl(button);

            return panel;
        }

        std::shared_ptr<Control> VesperGui::buildProgressBar(Form* form)
        {
            auto progressBarBg = std::make_shared<gui::Panel>(form);
            progressBarBg->setId("progressBarBg");
            progressBarBg->setPosition({ 0, 0 });
            progressBarBg->setSize({ 500, 20 });
            progressBarBg->setPadding({ 3, 3 });
            progressBarBg->setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
            progressBarBg->setAnchor(Anchor::BottomLeft);
            progressBarBg->setWidthSizingBehavior(Sizing::FillParent);

            auto progressBar = std::make_shared<gui::Panel>(form);
            progressBar->setId("progressBar");
            progressBar->setPosition({ 0, 0 });
            progressBar->setSize({ 500, 20 });
            progressBar->setPadding({ 3, 3 });
            progressBar->setColor(glm::vec4(0.1f, 0.5f, 0.1f, 1.0f));
            progressBar->setAnchor(Anchor::BottomLeft);
            progressBar->setWidthSizingBehavior(Sizing::FillParent, 0.0);
            progressBarBg->addControl(progressBar);

            auto label = std::make_shared<gui::Label>(form, "100.0%");
            label->setId("progressLabel");
            label->setPosition({ 6, 3 });
            label->setAnchor(Anchor::Center);
            label->setColor(glm::vec4(1.0f));
            progressBarBg->addControl(label);

            return progressBarBg;
        }
    }
}