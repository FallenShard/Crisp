#include "TopLevelGroup.hpp"

#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "ShaderLoader.hpp"
#include "FontLoader.hpp"

#include "DrawingVisitor.hpp"
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
        TopLevelGroup::TopLevelGroup()
            : m_guiHidden(false)
            , m_guiHidingEnabled(false)
            , m_prevMousePos(glm::vec2(-1.0f, -1.0f))
            , m_animator(std::make_unique<Animator>())
            , m_drawingVisitor(std::make_unique<DrawingVisitor>())
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            m_fontLoader = std::make_unique<FontLoader>();

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
        }

        void TopLevelGroup::setTracerProgress(float percentage, float timeSpentRendering)
        {
            float remainingPct = percentage == 0.0f ? 0.0f : (1.0f - percentage) / percentage * timeSpentRendering / 8.0f;

            std::stringstream stringStream;
            stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << percentage * 100 << " %    ETA: " << remainingPct << " seconds.";
            m_label->setText(stringStream.str());
        }

        void TopLevelGroup::draw()
        {
            if (m_mainGroup->needsValidation())
            {
                m_mainGroup->validate();
                m_mainGroup->setValidationStatus(true);
            }

            m_mainGroup->draw(*m_drawingVisitor);
        }

        void TopLevelGroup::resize(int width, int height)
        {
            m_drawingVisitor->setProjectionMarix(glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f));
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
            std::cout << "Mouse click position X:" << x << " Y:" << y << std::endl;

            m_mainGroup->onMousePressed(static_cast<float>(x), static_cast<float>(y));
        }

        void TopLevelGroup::onMouseReleased(int buttonId, int mods, double x, double y)
        {
            std::cout << "Mouse release position X:" << x << " Y:" << y << std::endl;

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
            m_fontLoader->load("SegoeUI.ttf", 12);
            auto& font = m_fontLoader->getFont("SegoeUI.ttf", 12);

            auto mainPanel = std::make_shared<gui::ControlGroup>();
            mainPanel->setId("TopLevelGroup");

            auto panel = std::make_shared<gui::Panel>();
            panel->setId("mainPanel");
            panel->setPosition({20, 20});
            panel->setPadding(glm::vec2(10.0f));
            panel->setSize({200, 500});
            mainPanel->addControl(panel);

            auto label = std::make_shared<gui::Label>(font);
            label->setId("fpsLabel");
            label->setPosition({0, 12});
            panel->addControl(label);

            auto button = std::make_shared<gui::Button>(font);
            button->setId("pupperButton");
            button->setPosition({0, 50});
            button->setSize({150, 30});
            panel->addControl(button);

            auto checkBox = std::make_shared<gui::CheckBox>(font);
            checkBox->setId("hideGuiCheckbox");
            checkBox->setPosition({0, 100});
            checkBox->setText("Hide GUI");
            panel->addControl(checkBox);

            return mainPanel;
        }
    }
}