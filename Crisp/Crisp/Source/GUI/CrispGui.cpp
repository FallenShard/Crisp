#include "CrispGui.hpp"

#include "Application.hpp"
#include "Core/Scene.hpp"

#include "Gui/Control.hpp"
#include "Gui/Panel.hpp"
#include "Gui/Button.hpp"
#include "Gui/Form.hpp"


namespace crisp
{
    namespace gui
    {
        void CrispGui::setupInputCallbacks(Form* form, Application* app, Scene* scene)
        {
            form->getControlById<gui::Button>("openButton")->setClickCallback([app]()
            {
                app->openSceneFileFromDialog();
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
            auto progressBar = form->getControlById<Panel>("progressBar");

            app->rayTracerProgressed.subscribe([form, progressLabel, progressBar](float percentage, float timeSpent)
            {
                float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpent / 8.0f;

                std::stringstream stringStream;
                stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100 << " %    ETA: " << remainingPct << " s";
                progressLabel->setText(stringStream.str());
                progressBar->setWidthSizingBehavior(Sizing::FillParent, percentage);
            });
        }

        std::shared_ptr<Control> CrispGui::buildCameraInfoPanel(Form* form)
        {
            auto panel = std::make_shared<gui::Panel>(form);
            panel->setId("cameraInfoPanel");
            panel->setPosition({ 10, 10 });
            panel->setSize({ 200, 500 });
            panel->setPadding({ 10, 10 });
            panel->setAnchor(Anchor::TopLeft);
            panel->setHeightSizingBehavior(Sizing::WrapContent);

            buildVectorDisplayInfo(form, panel, "Position", "cameraPositionLabel", 30.0f);
            buildVectorDisplayInfo(form, panel, "Direction", "cameraDirLabel", 70.0f);

            return panel;
        }

        void CrispGui::buildVectorDisplayInfo(Form* form, std::shared_ptr<ControlGroup> parent, std::string labelName, std::string dataLabelId, float verticalOffset)
        {
            auto propDescLabel = std::make_shared<gui::Label>(form, labelName);
            propDescLabel->setPosition({ 0.0f, verticalOffset });
            parent->addControl(propDescLabel);

            auto dataLabel = std::make_shared<gui::Label>(form, labelName);
            dataLabel->setPosition({ 0.0f, verticalOffset + 20.0f });
            dataLabel->setId(dataLabelId);
            parent->addControl(dataLabel);
        }
    }
}