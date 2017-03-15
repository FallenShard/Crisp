#pragma once

#include <memory>
#include <string>

#include "Math/Headers.hpp"

#include <Animation/Animator.hpp>
#include <GUI/ControlGroup.hpp>

namespace crisp
{
    class FontLoader;

    namespace gui
    {
        class CheckBox;
        class Label;
        class Button;
        class ControlGroup;
        class DrawingVisitor;
        class RenderSystem;
        class Panel;

        class Form
        {
        public:
            Form(std::unique_ptr<RenderSystem> drawingVisitor);
            ~Form();

            Form(const Form& other) = delete;
            Form& operator=(const Form& other) = delete;
            Form& operator=(Form&& other) = delete;

            RenderSystem* getRenderSystem();
            Animator* getAnimator();

            void postGuiUpdate(std::function<void()>&& guiUpdateCallback);

            void update(double dt);
            void setTracerProgress(float value, float timeSpentRendering);

            void resize(int width, int height);
            void onMouseMoved(double mouseX, double mouseY);
            void onMousePressed(int button, int mods, double mouseX, double mouseY);
            void onMouseReleased(int button, int mods, double mouseX, double mouseY);

            void draw();

            void setFpsString(const std::string& fps);

            template <typename T>
            T* getControlById(std::string id)
            {
                return m_rootControlGroup->getTypedControlById<T>(id);
            }

            void addMemoryUsagePanel();
            void addStatusBar();
            void add(std::shared_ptr<Control> control);
            
            void fadeOutAndRemove(std::string controlId, float duration = 1.0f);

        private:
            std::shared_ptr<ControlGroup> buildWelcomeScreen();
            std::shared_ptr<ControlGroup> buildStatusBar();
            std::shared_ptr<ControlGroup> buildMemoryUsagePanel();
            void buildProgressBar(std::shared_ptr<ControlGroup> parent, float verticalOffset, std::string name, std::string tag);
            std::shared_ptr<Control> fadeIn(std::shared_ptr<Control> controlGroup, float duration = 1.0f);
            void updateMemoryMetrics();

            glm::vec2 m_prevMousePos;
            bool m_guiHidden;
            bool m_guiHidingEnabled;

            double m_timePassed;

            std::unique_ptr<Animator> m_animator;
            std::unique_ptr<RenderSystem> m_renderSystem;

            std::shared_ptr<ControlGroup> m_rootControlGroup;

            Label* m_fpsLabel;
            Label* m_progressLabel;
            Panel* m_progressBar;
            Panel* m_bufferMemUsage;
            Label* m_bufferMemUsageLabel;
            Panel* m_imageMemUsage;
            Label* m_imageMemUsageLabel;
            Panel* m_stagingMemUsage;
            Label* m_stagingMemUsageLabel;

            std::vector<std::function<void()>> m_guiUpdates;
        };
    }
}