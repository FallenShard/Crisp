#include "CrispGui.hpp"

#include "Core/Application.hpp"
#include "Core/Scene.hpp"

#include "Gui/Control.hpp"
#include "Gui/Panel.hpp"
#include "Gui/Button.hpp"
#include "Gui/Form.hpp"
#include "GUI/Label.hpp"

namespace crisp::gui
{
    void CrispGui::setupInputCallbacks(Form* form, Application* app, Scene* scene)
    {
        form->getControlById<gui::Button>("openButton")->clicked += [app]()
        {
            app->openSceneFileFromDialog();
        };

        form->getControlById<gui::Button>("renderButton")->clicked += [app]()
        {
            app->startRayTracing();
        };

        form->getControlById<gui::Button>("stopButton")->clicked += [app]()
        {
            app->stopRayTracing();
        };

        form->getControlById<gui::Button>("saveButton")->clicked += [app]()
        {
            app->writeImageToExr();
        };

        auto progressLabel = form->getControlById<Label>("progressLabel");
        auto progressBar = form->getControlById<Panel>("progressBar");

        app->rayTracerProgressed.subscribe([form, progressLabel, progressBar](float percentage, float timeSpent)
        {
            float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpent / 8.0f;

            std::stringstream stringStream;
            stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100 << " %    ETA: " << remainingPct << " s";
            progressLabel->setText(stringStream.str());
            progressBar->setHorizontalSizingPolicy(SizingPolicy::FillParent, percentage);
        });
    }

    std::unique_ptr<Control> CrispGui::buildCameraInfoPanel(Form* form)
    {
        auto panel = std::make_unique<gui::Panel>(form);
        panel->setId("cameraInfoPanel");
        panel->setPosition({ 10, 10 });
        panel->setSizeHint({ 200, 500 });
        panel->setPadding({ 10, 10 });
        panel->setAnchor(Anchor::TopLeft);
        panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);

        buildVectorDisplayInfo(form, *panel, "Position", "cameraPositionLabel", 30.0f);
        buildVectorDisplayInfo(form, *panel, "Direction", "cameraDirLabel", 70.0f);

        return panel;
    }

    void CrispGui::buildVectorDisplayInfo(Form* form, ControlGroup& parent, std::string labelName, std::string&& dataLabelId, float verticalOffset)
    {
        auto propDescLabel = std::make_unique<gui::Label>(form, labelName);
        propDescLabel->setPosition({ 0.0f, verticalOffset });
        parent.addControl(std::move(propDescLabel));

        auto dataLabel = std::make_unique<gui::Label>(form, labelName);
        dataLabel->setPosition({ 0.0f, verticalOffset + 20.0f });
        dataLabel->setId(std::forward<std::string&&>(dataLabelId));
        parent.addControl(std::move(dataLabel));
    }
}