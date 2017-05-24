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
            , m_bufferMemUsageLabel(nullptr)
            , m_fpsLabel(nullptr)
        {
            m_rootControlGroup = std::make_shared<gui::ControlGroup>(this);
            m_rootControlGroup->setId("rootControlGroup");
            m_rootControlGroup->setDepthOffset(-32.0f);
            m_rootControlGroup->setSize(m_renderSystem->getScreenSize());

            m_rootControlGroup->addControl(fadeIn(buildWelcomeScreen()));
            
            m_fpsLabel             = m_rootControlGroup->getTypedControlById<Label>("fpsLabel");
            m_progressLabel        = m_rootControlGroup->getTypedControlById<Label>("progressLabel");
            m_progressBar          = m_rootControlGroup->getTypedControlById<Panel>("progressBar");
        }

        Form::~Form()
        {
        }

        RenderSystem* Form::getRenderSystem()
        {
            return m_renderSystem.get();
        }
        Animator* Form::getAnimator()
        {
            return m_animator.get();
        }

        void Form::postGuiUpdate(std::function<void()>&& guiUpdateCallback)
        {
            m_guiUpdates.emplace_back(std::forward<std::function<void()>>(guiUpdateCallback));
        }

        void Form::update(double dt)
        {
            if (!m_guiUpdates.empty())
            {
                for (auto& guiUpdate : m_guiUpdates)
                    guiUpdate();

                m_guiUpdates.clear();
            }
            
            m_animator->update(dt);

            m_timePassed += dt;

            if (m_timePassed > 1.0)
            {
                updateMemoryMetrics();
                m_timePassed -= 1.0;
            }
        }

        void Form::draw()
        {
            if (m_rootControlGroup->needsValidation())
            {
                m_rootControlGroup->validate();
                m_rootControlGroup->clearValidationFlags();
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
            if (m_fpsLabel)
                m_fpsLabel->setText(fps);
        }

        void Form::addMemoryUsagePanel()
        {
            m_rootControlGroup->addControl(fadeIn(buildMemoryUsagePanel()));
            m_bufferMemUsage       = m_rootControlGroup->getTypedControlById<Panel>("bufferDeviceMem");
            m_bufferMemUsageLabel  = m_rootControlGroup->getTypedControlById<Label>("bufferDeviceMemLabel");
            m_imageMemUsage        = m_rootControlGroup->getTypedControlById<Panel>("imageDeviceMem");
            m_imageMemUsageLabel   = m_rootControlGroup->getTypedControlById<Label>("imageDeviceMemLabel");
            m_stagingMemUsage      = m_rootControlGroup->getTypedControlById<Panel>("stagingMem");
            m_stagingMemUsageLabel = m_rootControlGroup->getTypedControlById<Label>("stagingMemLabel");
        }

        void Form::addStatusBar()
        {
            m_rootControlGroup->addControl(fadeIn(buildStatusBar()));
            m_fpsLabel = m_rootControlGroup->getTypedControlById<Label>("fpsLabel");
        }

        void Form::add(std::shared_ptr<Control> control)
        {
            m_rootControlGroup->addControl(fadeIn(control));
        }

        void Form::fadeOutAndRemove(std::string controlId, float duration)
        {
            auto control = m_rootControlGroup->getControlById(controlId);
            if (!control)
                return;

            auto colorAnim = std::make_shared<PropertyAnimation<float>>(duration, 1.0f, 0.0f, 0, Easing::SlowIn);
            colorAnim->setUpdater([control](const auto& t)
            {
                control->setOpacity(t);
            });
            colorAnim->finished.subscribe([this, control]()
            {
                postGuiUpdate([this, control]()
                {
                    m_rootControlGroup->removeControl(control->getId());
                });
            });
            m_animator->add(colorAnim);
        }

        std::shared_ptr<ControlGroup> Form::buildWelcomeScreen()
        {
            auto panel = std::make_shared<gui::Panel>(this);
            panel->setId("welcomePanel");
            panel->setSize({ 300, 500 });
            panel->setPadding({ 20, 20 });
            panel->setAnchor(Anchor::Center);
            panel->setHeightSizingBehavior(Sizing::WrapContent);
            panel->setWidthSizingBehavior(Sizing::WrapContent);
            panel->setOpacity(0.0f);

            auto welcomeLabel = std::make_shared<gui::Label>(this, "Welcome to Crisp!", 22);
            welcomeLabel->setPosition({ 0, 0 });
            welcomeLabel->setAnchor(Anchor::CenterHorizontally);
            panel->addControl(welcomeLabel);

            auto selectLabel = std::make_shared<gui::Label>(this, "Select your environment:");
            selectLabel->setPosition({ 0, 50 });
            selectLabel->setAnchor(Anchor::CenterHorizontally);
            panel->addControl(selectLabel);
            
            auto button = std::make_shared<gui::Button>(this);
            button->setId("crispButton");
            button->setText("Crisp (Real-time Renderer)");
            button->setFontSize(18);
            button->setPosition({ 0, 80 });
            button->setSize({ 250, 50 });
            button->setIdleColor({ 0.3f, 0.5f, 0.3f });
            button->setPressedColor({ 0.2f, 0.3f, 0.2f });
            button->setHoverColor({ 0.4f, 0.7f, 0.4f });
            button->setAnchor(Anchor::CenterHorizontally);
            panel->addControl(button);
            
            button = std::make_shared<gui::Button>(this);
            button->setId("vesperButton");
            button->setText("Vesper (Offline Ray Tracer)");
            button->setFontSize(18);
            button->setPosition({ 0, 150 });
            button->setSize({ 250, 50 });
            button->setIdleColor({ 0.3f, 0.3f, 0.5f });
            button->setPressedColor({ 0.2f, 0.2f, 0.3f });
            button->setHoverColor({ 0.4f, 0.4f, 0.7f });
            button->setAnchor(Anchor::CenterHorizontally);
            panel->addControl(button);

            auto cb = std::make_shared<gui::CheckBox>(this);
            cb->setId("fancyCB");
            cb->setPosition({ 20, 250 });
            cb->setText("Debug Check Box");
            panel->addControl(cb);

            return panel;
        }

        std::shared_ptr<ControlGroup> Form::buildStatusBar()
        {
            auto statusBar = std::make_shared<gui::Panel>(this);
            statusBar->setId("statusBar");
            statusBar->setPosition({ 0, 0 });
            statusBar->setSize({ 500, 20 });
            statusBar->setPadding({ 3, 3 });
            statusBar->setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
            statusBar->setWidthSizingBehavior(Sizing::FillParent);

            auto label = std::make_shared<gui::Label>(this, "ExampleText");
            label->setId("fpsLabel");
            label->setPosition({ 6, 3 });
            label->setAnchor(Anchor::Center);
            statusBar->addControl(label);

            return statusBar;
        }

        std::shared_ptr<ControlGroup> Form::buildMemoryUsagePanel()
        {
            auto panel = std::make_shared<gui::Panel>(this);
            panel->setId("memoryUsagePanel");
            panel->setPosition({ 10, 30 });
            panel->setSize({ 200, 300 });
            panel->setPadding({ 10, 10 });
            panel->setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
            panel->setAnchor(Anchor::BottomLeft);
            panel->setHeightSizingBehavior(Sizing::WrapContent);

            auto panelName = std::make_shared<gui::Label>(this, "Memory Usage");
            panelName->setPosition({ 0, 0 });
            panelName->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            panel->addControl(panelName);

            buildProgressBar(panel, 30.0f, "Buffer Device Memory", "bufferDeviceMem");
            buildProgressBar(panel, 90.0f, "Image Device Memory", "imageDeviceMem");
            buildProgressBar(panel, 150.0f, "Staging Memory", "stagingMem");

            return panel;
        }

        void Form::buildProgressBar(std::shared_ptr<ControlGroup> parent, float verticalOffset, std::string name, std::string tag)
        {
            auto bufferDevMemLabel = std::make_shared<gui::Label>(this, name);
            bufferDevMemLabel->setPosition({ 0, verticalOffset });
            bufferDevMemLabel->setColor(glm::vec4(1.0f));
            parent->addControl(bufferDevMemLabel);

            auto deviceMemoryBg = std::make_shared<gui::Panel>(this);
            deviceMemoryBg->setPosition({ 0, verticalOffset + 20.0f });
            deviceMemoryBg->setSize({ 200, 20 });
            deviceMemoryBg->setPadding({ 3, 3 });
            deviceMemoryBg->setColor(glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
            parent->addControl(deviceMemoryBg);

            auto deviceMemory = std::make_shared<gui::Panel>(this);
            deviceMemory->setId(tag);
            deviceMemory->setPosition({ 0, 0 });
            deviceMemory->setSize({ 200, 20 });
            deviceMemory->setColor(glm::vec4(0.0f, 0.5f, 0.0f, 1.0f));
            deviceMemory->setWidthSizingBehavior(Sizing::FillParent, 0.3f);
            deviceMemoryBg->addControl(deviceMemory);

            auto bufferMemLabel = std::make_shared<gui::Label>(this, "100.0%");
            bufferMemLabel->setId(tag + "Label");
            bufferMemLabel->setAnchor(Anchor::Center);
            bufferMemLabel->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            deviceMemoryBg->addControl(bufferMemLabel);
        }

        std::shared_ptr<Control> Form::fadeIn(std::shared_ptr<Control> control, float duration)
        {
            auto anim = std::make_shared<PropertyAnimation<float>>(duration, 0.0f, 1.0f, 0, Easing::SlowOut);
            anim->setUpdater([this, control](const auto& t)
            {
                control->setOpacity(t);
            });
            m_animator->add(anim);

            return control;
        }

        void Form::updateMemoryMetrics()
        {
            if (!m_bufferMemUsageLabel)
                return;

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
        }
    }
}