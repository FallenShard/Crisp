#include "VesperGui.hpp"

#include "Core/Application.hpp"
#include "Control.hpp"
#include "Panel.hpp"
#include "Button.hpp"
#include "Form.hpp"
#include "Label.hpp"

namespace crisp
{
    namespace gui
    {
        void VesperGui::setupInputCallbacks(Form* form, Application* app)
        {
            auto progressLabel = form->getControlById<Label>("progressLabel");
            auto progressBar   = form->getControlById<Panel>("progressBar");

            //app->rayTracerProgressed.subscribe([form, progressLabel, progressBar](float percentage, float timeSpent)
            //{
            //    float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpent / 8.0f;
            //    
            //    std::stringstream stringStream;
            //    stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100 << " %    ETA: " << remainingPct << " s";
            //    progressLabel->setText(stringStream.str());
            //    progressBar->setHorizontalSizingPolicy(SizingPolicy::FillParent, percentage);
            //});
        }

        std::unique_ptr<Control> VesperGui::buildSceneOptions(Form* form)
        {
            auto panel = std::make_unique<gui::Panel>(form);
            panel->setId("vesperOptionsPanel");
            panel->setPosition({ 10, 30 });
            panel->setSizeHint({ 200, 500 });
            panel->setPadding({ 10, 10 });
            panel->setAnchor(Anchor::TopLeft);
            panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);

            auto button = std::make_unique<gui::Button>(form);
            button->setId("openButton");
            button->setText("Open XML Scene");
            button->setSizeHint({ 100, 30 });
            button->setHorizontalSizingPolicy(SizingPolicy::FillParent);
            panel->addControl(std::move(button));

            button = std::make_unique<gui::Button>(form);
            button->setId("renderButton");
            button->setText("Start Raytracing");
            button->setPosition({ 0, 40 });
            button->setSizeHint({ 100, 30 });
            button->setHorizontalSizingPolicy(SizingPolicy::FillParent);
            panel->addControl(std::move(button));

            button = std::make_unique<gui::Button>(form);
            button->setId("stopButton");
            button->setText("Stop Raytracing");
            button->setPosition({ 0, 80 });
            button->setSizeHint({ 100, 30 });
            button->setHorizontalSizingPolicy(SizingPolicy::FillParent);
            panel->addControl(std::move(button));

            button = std::make_unique<gui::Button>(form);
            button->setId("saveButton");
            button->setText("Save as EXR");
            button->setPosition({ 0, 120 });
            button->setSizeHint({ 100, 30 });
            button->setHorizontalSizingPolicy(SizingPolicy::FillParent);
            panel->addControl(std::move(button));

            return panel;
        }

        std::unique_ptr<Control> VesperGui::buildProgressBar(Form* form)
        {
            auto progressBarBg = std::make_unique<gui::Panel>(form);
            progressBarBg->setId("progressBarBg");
            progressBarBg->setPosition({ 0, 0 });
            progressBarBg->setSizeHint({ 500, 20 });
            progressBarBg->setPadding({ 3, 3 });
            progressBarBg->setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
            progressBarBg->setAnchor(Anchor::BottomRight);
            progressBarBg->setHorizontalSizingPolicy(SizingPolicy::FillParent, 0.5f);

            auto progressBar = std::make_unique<gui::Panel>(form);
            progressBar->setId("progressBar");
            progressBar->setPosition({ 0, 0 });
            progressBar->setSizeHint({ 500, 20 });
            progressBar->setPadding({ 3, 3 });
            progressBar->setColor(glm::vec4(0.1f, 0.5f, 0.1f, 1.0f));
            progressBar->setAnchor(Anchor::BottomLeft);
            progressBar->setHorizontalSizingPolicy(SizingPolicy::FillParent, 0.0);
            progressBarBg->addControl(std::move(progressBar));

            auto label = std::make_unique<gui::Label>(form, "100.0%");
            label->setId("progressLabel");
            label->setAnchor(Anchor::Center);
            label->setColor(glm::vec4(1.0f));
            progressBarBg->addControl(std::move(label));

            return progressBarBg;
        }
    }
}