#include "Form.hpp"

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
        Form::Form(std::unique_ptr<RenderSystem> renderSystem)
            : m_guiHidden(false)
            , m_guiHidingEnabled(false)
            , m_prevMousePos(glm::vec2(-1.0f, -1.0f))
            , m_animator(std::make_unique<Animator>())
            , m_renderSystem(std::move(renderSystem))
            , m_timePassed(0.0)
        {
            m_rootControlGroup = buildGui();
            
            m_optionsPanel         = m_rootControlGroup->getTypedControlById<Panel>("mainPanel");
            m_fpsLabel             = m_rootControlGroup->getTypedControlById<Label>("fpsLabel");
            m_progressLabel        = m_rootControlGroup->getTypedControlById<Label>("progressLabel");
            m_progressBar          = m_rootControlGroup->getTypedControlById<Panel>("progressBar");
            m_bufferMemUsage       = m_rootControlGroup->getTypedControlById<Panel>("bufferDeviceMem");
            m_bufferMemUsageLabel  = m_rootControlGroup->getTypedControlById<Label>("bufferDeviceMemLabel");
            m_imageMemUsage        = m_rootControlGroup->getTypedControlById<Panel>("imageDeviceMem");
            m_imageMemUsageLabel   = m_rootControlGroup->getTypedControlById<Label>("imageDeviceMemLabel");
            m_stagingMemUsage      = m_rootControlGroup->getTypedControlById<Panel>("stagingMem");
            m_stagingMemUsageLabel = m_rootControlGroup->getTypedControlById<Label>("stagingMemLabel");

            //
            //auto checkBox = m_mainGroup->getTypedControlById<gui::CheckBox>("hideGuiCheckbox");
            //checkBox->setCheckCallback([this](bool isChecked)
            //{
            //    m_guiHidingEnabled = isChecked;
            //});
        }

        Form::~Form()
        {
        }

        void Form::update(double dt)
        {
            for (auto& controlPair : m_pendingControls)
            {
                controlPair.first->addControl(controlPair.second);
            }
            m_pendingControls.clear();

            
            m_animator->update(dt);

            m_timePassed += dt;

            if (m_timePassed > 1.0)
            {
                auto metrics = m_renderSystem->getDeviceMemoryUsage();

                // Device buffer
                float percentage = static_cast<float>(metrics.bufferMemoryUsed) / static_cast<float>(metrics.bufferMemorySize);

                std::stringstream deviceBufferStream;
                deviceBufferStream << std::to_string(metrics.bufferMemoryUsed >> 20) << " / " << std::to_string(metrics.bufferMemorySize >> 20) << " MB";

                m_bufferMemUsageLabel->setText(deviceBufferStream.str());
                m_bufferMemUsage->setWidthSizingBehavior(Sizing::FillParent, percentage);

                // Device image
                percentage = static_cast<float>(metrics.imageMemoryUsed) / static_cast<float>(metrics.imageMemorySize);

                std::stringstream deviceImageStream;
                deviceImageStream << std::to_string(metrics.imageMemoryUsed >> 20) << " / " << std::to_string(metrics.imageMemorySize >> 20) << " MB";

                m_imageMemUsageLabel->setText(deviceImageStream.str());
                m_imageMemUsage->setWidthSizingBehavior(Sizing::FillParent, percentage);

                percentage = static_cast<float>(metrics.stagingMemoryUsed) / static_cast<float>(metrics.stagingMemorySize);

                std::stringstream stagingMemoryStream;
                stagingMemoryStream << std::to_string(metrics.stagingMemoryUsed >> 20) << " / " << std::to_string(metrics.stagingMemorySize >> 20) << " MB";

                m_stagingMemUsageLabel->setText(stagingMemoryStream.str());
                m_stagingMemUsage->setWidthSizingBehavior(Sizing::FillParent, percentage);

                m_timePassed -= 1.0;
            }
        }

        void Form::setTracerProgress(float percentage, float timeSpentRendering)
        {
            float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpentRendering / 8.0f;
            
            std::stringstream stringStream;
            stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100 << " %    ETA: " << remainingPct << " s";
            m_progressLabel->setText(stringStream.str());
            m_progressBar->setWidthSizingBehavior(Sizing::FillParent, percentage);
        }

        void Form::draw()
        {
            if (m_rootControlGroup->isInvalidated())
            {
                m_rootControlGroup->validate();
                m_rootControlGroup->setValidationStatus(true);
            }

            m_rootControlGroup->draw(*m_renderSystem);
            m_renderSystem->submitDrawRequests();
        }

        void Form::resize(int width, int height)
        {
            m_renderSystem->resize(width, height);
            m_rootControlGroup->setSize({ width, height });
        }

        void Form::onMouseMoved(double x, double y)
        {
            m_rootControlGroup->onMouseMoved(static_cast<float>(x), static_cast<float>(y));
            
            //if (m_guiHidingEnabled && m_guiHidden && x < 200)
            //{
            //
            //    glm::vec2 start = m_panel->getPosition();
            //    glm::vec2 end(20, 20);
            //    auto showAnim = std::make_shared<PropertyAnimation<glm::vec2>>(0.5, start, end, 0, Easing::SlowOut);
            //    showAnim->setUpdater([this](const glm::vec2& t)
            //    {
            //        m_panel->setPosition(t);
            //    });
            //    m_animator->add(showAnim);
            //    m_guiHidden = false;
            //}
            //
            //if (m_guiHidingEnabled && !m_guiHidden && x > 200)
            //{
            //    glm::vec2 start = m_panel->getPosition();
            //    glm::vec2 end(-500, 20);
            //    auto hideAnim = std::make_shared<PropertyAnimation<glm::vec2>>(0.5, start, end, 0, Easing::SlowIn);
            //    hideAnim->setUpdater([this](const glm::vec2& t)
            //    {
            //        m_panel->setPosition(t);
            //    });
            //    m_animator->add(hideAnim);
            //    m_guiHidden = true;
            //}
            
            m_prevMousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
        }

        void Form::onMousePressed(int buttonId, int mods, double x, double y)
        {
            m_rootControlGroup->onMousePressed(static_cast<float>(x), static_cast<float>(y));
        }

        void Form::onMouseReleased(int buttonId, int mods, double x, double y)
        {
            m_rootControlGroup->onMouseReleased(static_cast<float>(x), static_cast<float>(y));
        }

        void Form::setFpsString(const std::string& fps)
        {
            m_fpsLabel->setText(fps);
        }

        std::shared_ptr<ControlGroup> Form::buildGui()
        {
            auto rootControlGroup = std::make_shared<gui::ControlGroup>();
            rootControlGroup->setId("rootControlGroup");
            rootControlGroup->setDepthOffset(-32.0f);
            rootControlGroup->setSize(m_renderSystem->getScreenSize());

            rootControlGroup->addControl(buildStatusBar());
            rootControlGroup->addControl(buildSceneOptions());
            rootControlGroup->addControl(buildProgressBar());
            rootControlGroup->addControl(buildMemoryUsagePanel());

            /*
            
            auto checkBox = std::make_shared<gui::CheckBox>(m_renderSystem.get());
            checkBox->setId("hideGuiCheckbox");
            checkBox->setPosition({0, currY });
            checkBox->setText("Hide GUI");
            panel->addControl(checkBox);
            currY += 40.0f;

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
            currY += 20.0f;*/

            return rootControlGroup;
        }

        std::shared_ptr<ControlGroup> Form::buildStatusBar()
        {
            auto statusBar = std::make_shared<gui::Panel>(m_renderSystem.get());
            statusBar->setId("statusBar");
            statusBar->setPosition({ 0, 0 });
            statusBar->setSize({ 500, 20 });
            statusBar->setPadding({ 3, 3 });
            statusBar->setColor(ColorPalette::DarkGray);
            statusBar->setWidthSizingBehavior(Sizing::FillParent);

            auto label = std::make_shared<gui::Label>(m_renderSystem.get(), "ExampleText");
            label->setId("fpsLabel");
            label->setPosition({ 6, 3 });
            label->setAnchor(Anchor::TopRight);
            statusBar->addControl(label);

            return statusBar;
        }

        std::shared_ptr<ControlGroup> Form::buildSceneOptions()
        {
            auto panel = std::make_shared<gui::Panel>(m_renderSystem.get());
            panel->setId("optionsPanel");
            panel->setPosition({ 10, 30 });
            panel->setSize({ 200, 500 });
            panel->setPadding({ 10, 10 });
            panel->setAnchor(Anchor::TopLeft);
            panel->setHeightSizingBehavior(Sizing::WrapContent);
            
            auto button = std::make_shared<gui::Button>(m_renderSystem.get());
            button->setId("openButton");
            button->setText("Open XML Scene");
            button->setSize({ 100, 30 });
            button->setWidthSizingBehavior(Sizing::FillParent);
            panel->addControl(button);

            button = std::make_shared<gui::Button>(m_renderSystem.get());
            button->setId("renderButton");
            button->setText("Start Raytracing");
            button->setPosition({ 0, 50 });
            button->setSize({ 100, 30 });
            button->setWidthSizingBehavior(Sizing::FillParent);
            panel->addControl(button);

            button = std::make_shared<gui::Button>(m_renderSystem.get());
            button->setId("stopButton");
            button->setText("Stop Raytracing");
            button->setPosition({ 0, 90 });
            button->setSize({ 100, 30 });
            button->setWidthSizingBehavior(Sizing::FillParent);
            panel->addControl(button);

            button = std::make_shared<gui::Button>(m_renderSystem.get());
            button->setId("saveButton");
            button->setText("Save as EXR");
            button->setPosition({ 0, 130 });
            button->setSize({ 100, 30 });
            button->setWidthSizingBehavior(Sizing::FillParent);
            panel->addControl(button);

            return panel;
        }

        std::shared_ptr<ControlGroup> Form::buildProgressBar()
        {
            auto progressBarBg = std::make_shared<gui::Panel>(m_renderSystem.get());
            progressBarBg->setId("progressBarBg");
            progressBarBg->setPosition({ 0, 0 });
            progressBarBg->setSize({ 500, 20 });
            progressBarBg->setPadding({ 3, 3 });
            progressBarBg->setColor(ColorPalette::DarkGray);
            progressBarBg->setAnchor(Anchor::BottomLeft);
            progressBarBg->setWidthSizingBehavior(Sizing::FillParent);

            auto progressBar = std::make_shared<gui::Panel>(m_renderSystem.get());
            progressBar->setId("progressBar");
            progressBar->setPosition({ 0, 0 });
            progressBar->setSize({ 500, 20 });
            progressBar->setPadding({ 3, 3 });
            progressBar->setColor(ColorPalette::DarkGreen);
            progressBar->setAnchor(Anchor::BottomLeft);
            progressBar->setWidthSizingBehavior(Sizing::FillParent, 0.0);
            progressBarBg->addControl(progressBar);

            auto label = std::make_shared<gui::Label>(m_renderSystem.get(), "100.0%");
            label->setId("progressLabel");
            label->setPosition({ 6, 3 });
            label->setAnchor(Anchor::Center);
            label->setColor(ColorPalette::White);
            progressBarBg->addControl(label);

            return progressBarBg;
        }

        std::shared_ptr<ControlGroup> Form::buildMemoryUsagePanel()
        {
            auto panel = std::make_shared<gui::Panel>(m_renderSystem.get());
            panel->setId("memoryUsagePanel");
            panel->setPosition({ 10, 30 });
            panel->setSize({ 200, 300 });
            panel->setPadding({ 10, 10 });
            panel->setColor(ColorPalette::DarkGray);
            panel->setAnchor(Anchor::BottomLeft);

            auto panelName = std::make_shared<gui::Label>(m_renderSystem.get(), "Memory Usage");
            panelName->setPosition({ 0, 0 });
            panelName->setColor(ColorPalette::Green);
            panel->addControl(panelName);

            buildProgressBar(panel, 50.0f, "Buffer Device Memory", "bufferDeviceMem");
            buildProgressBar(panel, 110.0f, "Image Device Memory", "imageDeviceMem");
            buildProgressBar(panel, 160.0f, "Staging Memory", "stagingMem");

            //auto bufferDevMemLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "Buffer Device Memory");
            //bufferDevMemLabel->setPosition({ 0, 50 });
            //bufferDevMemLabel->setColor(ColorPalette::White);
            //panel->addControl(bufferDevMemLabel);
            //
            //auto deviceMemoryBg = std::make_shared<gui::Panel>(m_renderSystem.get());
            //deviceMemoryBg->setPosition({ 0, 70 });
            //deviceMemoryBg->setSize({ 200, 20 });
            //deviceMemoryBg->setPadding({ 3, 3 });
            //deviceMemoryBg->setColor(ColorPalette::Gray20);
            //panel->addControl(deviceMemoryBg);
            //
            //auto deviceMemory = std::make_shared<gui::Panel>(m_renderSystem.get());
            //deviceMemory->setId("bufferMemUsage");
            //deviceMemory->setPosition({ 0, 0 });
            //deviceMemory->setSize({ 200, 20 });
            //deviceMemory->setColor(ColorPalette::DarkGreen);
            //deviceMemory->setWidthSizingBehavior(Sizing::FillParent, 0.3f);
            //deviceMemoryBg->addControl(deviceMemory);
            //
            //auto bufferMemLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "100.0%");
            //bufferMemLabel->setId("bufferMemUsageLabel");
            //bufferMemLabel->setPosition({ 6, 3 });
            //bufferMemLabel->setAnchor(Anchor::Center);
            //bufferMemLabel->setColor(ColorPalette::White);
            //deviceMemoryBg->addControl(bufferMemLabel);

            //auto imageDeviceMemory = std::make_shared<gui::Label>(m_renderSystem.get(), "Image Device Memory");
            //imageDeviceMemory->setPosition({ 0, 110 });
            //imageDeviceMemory->setColor(ColorPalette::White);
            //panel->addControl(imageDeviceMemory);

            return panel;
        }

        void Form::buildProgressBar(std::shared_ptr<ControlGroup> parent, float verticalOffset, std::string name, std::string tag)
        {
            auto bufferDevMemLabel = std::make_shared<gui::Label>(m_renderSystem.get(), name);
            bufferDevMemLabel->setPosition({ 0, verticalOffset });
            bufferDevMemLabel->setColor(ColorPalette::White);
            parent->addControl(bufferDevMemLabel);

            auto deviceMemoryBg = std::make_shared<gui::Panel>(m_renderSystem.get());
            deviceMemoryBg->setPosition({ 0, verticalOffset + 20.0f });
            deviceMemoryBg->setSize({ 200, 20 });
            deviceMemoryBg->setPadding({ 3, 3 });
            deviceMemoryBg->setColor(ColorPalette::Gray20);
            parent->addControl(deviceMemoryBg);

            auto deviceMemory = std::make_shared<gui::Panel>(m_renderSystem.get());
            deviceMemory->setId(tag);
            deviceMemory->setPosition({ 0, 0 });
            deviceMemory->setSize({ 200, 20 });
            deviceMemory->setColor(ColorPalette::DarkGreen);
            deviceMemory->setWidthSizingBehavior(Sizing::FillParent, 0.3f);
            deviceMemoryBg->addControl(deviceMemory);

            auto bufferMemLabel = std::make_shared<gui::Label>(m_renderSystem.get(), "100.0%");
            bufferMemLabel->setId(tag + "Label");
            bufferMemLabel->setAnchor(Anchor::Center);
            bufferMemLabel->setColor(ColorPalette::White);
            deviceMemoryBg->addControl(bufferMemLabel);
        }
    }
}