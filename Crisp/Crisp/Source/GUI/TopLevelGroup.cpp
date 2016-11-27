#include "TopLevelGroup.hpp"

#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "ShaderLoader.hpp"
#include "FontLoader.hpp"

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

            m_panel = m_mainGroup->getTypedControlById<Panel>("mainPanel");

            m_button = m_mainGroup->getTypedControlById<gui::Button>("pupperButton");
            m_button->setClickCallback([this]()
            {
                std::cout << "Vilasini is a pupper." << std::endl;
            });
            
            m_label = m_mainGroup->getTypedControlById<gui::Label>("fpsLabel");
            
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
            m_animator->update(dt);

            auto usage = m_renderSystem->getDeviceMemoryUsage();
            float percentage = static_cast<float>(usage.second) / static_cast<float>(usage.first);
            auto label = m_mainGroup->getTypedControlById<Label>("memUsageLabel");
            label->setText("GPU memory: " + std::to_string(usage.second >> 20) + " / " + std::to_string(usage.first >> 20) + " MB");

            auto panel = m_mainGroup->getTypedControlById<Panel>("memUsageFg");
            panel->setSize({ 146.0f * percentage, 26.0f });
        }

        void TopLevelGroup::setTracerProgress(float percentage, float timeSpentRendering)
        {
            float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpentRendering / 8.0f;
            
            std::stringstream stringStream;
            stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100 << " %    ETA: " << remainingPct << " s";
            m_label->setText(stringStream.str());
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
            m_label->setText(fps);
        }

        gui::Button* TopLevelGroup::getButton()
        {
            return m_button;
        }

        std::shared_ptr<ControlGroup> TopLevelGroup::buildGui()
        {
            auto topLevelGroup = std::make_shared<gui::ControlGroup>();
            topLevelGroup->setId("TopLevelGroup");

            auto panel = std::make_shared<gui::Panel>(m_renderSystem.get());
            panel->setId("mainPanel");
            panel->setPosition({20, 20});
            panel->setPadding(glm::vec2(10.0f));
            panel->setSize({200, 500});
            topLevelGroup->addControl(panel);
            
            auto label = std::make_shared<gui::Label>(m_renderSystem.get());
            label->setId("fpsLabel");
            label->setPosition({0, 12});
            panel->addControl(label);
            
            auto button = std::make_shared<gui::Button>(m_renderSystem.get());
            button->setId("pupperButton");
            button->setPosition({0, 50});
            button->setSize({150, 30});
            panel->addControl(button);
            
            auto checkBox = std::make_shared<gui::CheckBox>(m_renderSystem.get());
            checkBox->setId("hideGuiCheckbox");
            checkBox->setPosition({0, 100});
            checkBox->setText("Hide GUI");
            panel->addControl(checkBox);

            auto backgroundProgress = std::make_shared<gui::Panel>(m_renderSystem.get());
            backgroundProgress->setId("memUsageBg");
            backgroundProgress->setPosition({ 0, 150 });
            backgroundProgress->setPadding(glm::vec2(2.0f));
            backgroundProgress->setSize({ 150, 30 });
            backgroundProgress->useAbsoluteSizing(true);
            backgroundProgress->setColor(DarkGreen);
            panel->addControl(backgroundProgress);

            auto memUsageLabel = std::make_shared<gui::Label>(m_renderSystem.get());
            memUsageLabel->setId("memUsageLabel");
            memUsageLabel->setPosition({ 0, 140 });
            memUsageLabel->setColor(DarkGreen);
            memUsageLabel->setScale(0.8f);
            panel->addControl(memUsageLabel);

            auto memUsage = std::make_shared<gui::Panel>(m_renderSystem.get());
            memUsage->setId("memUsageFg");
            memUsage->setPosition({ 0, 0 });
            memUsage->setSize({ 55.0f, 26.0f });
            memUsage->useAbsoluteSizing(true);
            memUsage->setColor(Green);
            backgroundProgress->addControl(memUsage);

            m_renderSystem->buildResourceBuffers();

            return topLevelGroup;
        }
    }
}