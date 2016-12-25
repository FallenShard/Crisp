#include "TopLevelGroup.hpp"

#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "RenderSystem.hpp"
#include "ControlGroup.hpp"
#include "Button.hpp"
#include "Label.hpp"
#include "CheckBox.hpp"
#include "Control.hpp"
#include "Panel.hpp"

#include "Animation/PropertyAnimation.hpp"

namespace crisp
{
    namespace gui
    {
        TopLevelGroup::TopLevelGroup(std::unique_ptr<RenderSystem> renderSystem)
            : m_guiHidden(false)
            , m_guiHidingEnabled(false)
            , m_prevMousePos(glm::vec2(-1.0f, -1.0f))
            , m_animator(std::make_unique<Animator>())
            , m_renderSystem(std::move(renderSystem))
        {
            m_mainGroup = buildGui();
            m_renderSystem->buildResourceBuffers();

            m_panel = m_mainGroup->getTypedControlById<Panel>("mainPanel");
            
            m_fpsLabel = m_mainGroup->getTypedControlById<gui::Label>("fpsLabel");
            m_progressLabel = m_mainGroup->getTypedControlById<gui::Label>("progressLabel");
            
            auto checkBox = m_mainGroup->getTypedControlById<gui::CheckBox>("hideGuiCheckbox");
            checkBox->setCheckCallback([this](bool isChecked)
            {
                m_guiHidingEnabled = isChecked;
            });
        }

        TopLevelGroup::~TopLevelGroup()
        {
        }

        void TopLevelGroup::update(double dt)
        {
            static double time = 0.0;
            m_animator->update(dt);

            if (time > 1.0)
            {
                auto usage = m_renderSystem->getDeviceMemoryUsage();
                float percentage = static_cast<float>(usage.second) / static_cast<float>(usage.first);
                auto label = m_mainGroup->getTypedControlById<Label>("memUsageLabel");
                label->setText("GPU memory: " + std::to_string(usage.second >> 20) + " / " + std::to_string(usage.first >> 20) + " MB");

                auto panel = m_mainGroup->getTypedControlById<Panel>("memUsageFg");
                panel->setSize({ 146.0f * percentage, 26.0f });

                time -= 1.0;
            }
            time += dt;
        }

        void TopLevelGroup::setTracerProgress(float percentage, float timeSpentRendering)
        {
            float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpentRendering / 8.0f;
            
            std::stringstream stringStream;
            stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100 << " %    ETA: " << remainingPct << " s";
            m_progressLabel->setText(stringStream.str());
        }

        void TopLevelGroup::draw()
        {
            if (m_mainGroup->needsValidation())
            {
                m_mainGroup->validate();
                m_mainGroup->setValidationStatus(true);
            }

            m_mainGroup->draw(*m_renderSystem);
            m_renderSystem->submitDrawRequests();
        }

        void TopLevelGroup::resize(int width, int height)
        {
            m_renderSystem->resize(width, height);
            m_mainGroup->invalidate();
        }

        void TopLevelGroup::onMouseMoved(double x, double y)
        {
            m_mainGroup->onMouseMoved(static_cast<float>(x), static_cast<float>(y));

            if (m_guiHidingEnabled && m_guiHidden && x < 200)
            {

                glm::vec2 start = m_panel->getPosition();
                glm::vec2 end(20, 20);
                auto showAnim = std::make_shared<PropertyAnimation<glm::vec2>>(0.5, start, end, 0, Easing::SlowOut);
                showAnim->setUpdater([this](const glm::vec2& t)
                {
                    m_panel->setPosition(t);
                });
                m_animator->add(showAnim);
                m_guiHidden = false;
            }

            if (m_guiHidingEnabled && !m_guiHidden && x > 200)
            {
                glm::vec2 start = m_panel->getPosition();
                glm::vec2 end(-500, 20);
                auto hideAnim = std::make_shared<PropertyAnimation<glm::vec2>>(0.5, start, end, 0, Easing::SlowIn);
                hideAnim->setUpdater([this](const glm::vec2& t)
                {
                    m_panel->setPosition(t);
                });
                m_animator->add(hideAnim);
                m_guiHidden = true;
            }

            m_prevMousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
        }

        void TopLevelGroup::onMousePressed(int buttonId, int mods, double x, double y)
        {
            m_mainGroup->onMousePressed(static_cast<float>(x), static_cast<float>(y));
        }

        void TopLevelGroup::onMouseReleased(int buttonId, int mods, double x, double y)
        {
            m_mainGroup->onMouseReleased(static_cast<float>(x), static_cast<float>(y));
        }

        void TopLevelGroup::setFpsString(const std::string& fps)
        {
            m_fpsLabel->setText(fps);
        }

        std::shared_ptr<ControlGroup> TopLevelGroup::buildGui()
        {
            auto topLevelGroup = std::make_shared<gui::ControlGroup>();
            topLevelGroup->setId("TopLevelGroup");

            auto panel = std::make_shared<gui::Panel>(m_renderSystem.get());
            panel->setId("mainPanel");
            panel->setPosition({ 20, 20 });
            panel->setPadding(glm::vec2(10.0f));
            panel->setSize({200, 500});
            topLevelGroup->addControl(panel);
            
            float currY = 10.0f;
            auto label = std::make_shared<gui::Label>(m_renderSystem.get());
            label->setId("fpsLabel");
            label->setPosition({ 0, currY });
            panel->addControl(label);
            currY += 20.0f;

            auto progLabel = std::make_shared<gui::Label>(m_renderSystem.get());
            progLabel->setId("progressLabel");
            progLabel->setText("PROGRESS");
            progLabel->setPosition({ 0, currY });
            panel->addControl(progLabel);
            currY += 20.0f;
            
            auto openButton = std::make_shared<gui::Button>(m_renderSystem.get());
            openButton->setText("Open Scene");
            openButton->setId("openProjectButton");
            openButton->setPosition({0, currY });
            openButton->setSize({150, 30});
            panel->addControl(openButton);
            currY += 40.0f;

            auto startButton = std::make_shared<gui::Button>(m_renderSystem.get());
            startButton->setText("Start Raytracing");
            startButton->setId("startRaytracingButton");
            startButton->setPosition({ 0, currY });
            startButton->setSize({ 150, 30 });
            panel->addControl(startButton);
            currY += 40.0f;

            auto stopButton = std::make_shared<gui::Button>(m_renderSystem.get());
            stopButton->setText("Stop");
            stopButton->setId("stopRaytracingButton");
            stopButton->setPosition({ 0, currY });
            stopButton->setSize({ 150, 30 });
            panel->addControl(stopButton);
            currY += 50.0f;
            
            auto checkBox = std::make_shared<gui::CheckBox>(m_renderSystem.get());
            checkBox->setId("hideGuiCheckbox");
            checkBox->setPosition({0, currY });
            checkBox->setText("Hide GUI");
            panel->addControl(checkBox);
            currY += 40.0f;

            auto memUsageLabel = std::make_shared<gui::Label>(m_renderSystem.get());
            memUsageLabel->setId("memUsageLabel");
            memUsageLabel->setPosition({ 0, currY });
            memUsageLabel->setColor(DarkGreen);
            memUsageLabel->setScale(0.8f);
            panel->addControl(memUsageLabel);
            currY += 20.0f;

            auto backgroundProgress = std::make_shared<gui::Panel>(m_renderSystem.get());
            backgroundProgress->setId("memUsageBg");
            backgroundProgress->setPosition({ 0, currY });
            backgroundProgress->setPadding(glm::vec2(2.0f));
            backgroundProgress->setSize({ 150, 30 });
            backgroundProgress->useAbsoluteSizing(true);
            backgroundProgress->setColor(DarkGreen);
            panel->addControl(backgroundProgress);
            currY += 55.0f;

            auto memUsage = std::make_shared<gui::Panel>(m_renderSystem.get());
            memUsage->setId("memUsageFg");
            memUsage->setPosition({ 0, 0 });
            memUsage->setSize({ 0.0f, 26.0f });
            memUsage->useAbsoluteSizing(true);
            memUsage->setColor(Green);
            backgroundProgress->addControl(memUsage);

            auto cameraLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "Camera");
            cameraLabel->setPosition({ 0, currY });
            cameraLabel->setColor(Green);
            panel->addControl(cameraLabel);
            currY += 25.0f;

            auto camPosLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "Position:");
            camPosLabel->setPosition({ 0, currY });
            camPosLabel->setColor(DarkGreen);
            panel->addControl(camPosLabel);

            auto camPosValueLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "0.00, 0.05, 0.08");
            camPosValueLabel->setId("camPosValueLabel");
            camPosValueLabel->setPosition({ 80, currY });
            camPosValueLabel->setColor(Green);
            panel->addControl(camPosValueLabel);
            currY += 20.0f;

            auto camOrientationLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "Orientation:");
            camOrientationLabel->setPosition({ 0, currY });
            camOrientationLabel->setColor(DarkGreen);
            panel->addControl(camOrientationLabel);

            auto camOrientValueLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "0.00, 0.05, 0.06");
            camOrientValueLabel->setId("camOrientValueLabel");
            camOrientValueLabel->setPosition({ 80, currY });
            camOrientValueLabel->setColor(Green);
            panel->addControl(camOrientValueLabel);
            currY += 20.0f;

            auto camUpLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "Up direction:");
            camUpLabel->setPosition({ 0, currY });
            camUpLabel->setColor(DarkGreen);
            panel->addControl(camUpLabel);

            auto camUpValueLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "0.00, 0.05, 0.06");
            camUpValueLabel->setId("camUpValueLabel");
            camUpValueLabel->setPosition({ 80, currY });
            camUpValueLabel->setColor(Green);
            panel->addControl(camUpValueLabel);
            currY += 20.0f;

            return topLevelGroup;
        }
    }
}