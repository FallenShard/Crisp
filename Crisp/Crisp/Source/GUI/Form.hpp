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

        private:
            std::shared_ptr<ControlGroup> buildGui();
            std::shared_ptr<ControlGroup> buildStatusBar();
            std::shared_ptr<ControlGroup> buildSceneOptions();
            std::shared_ptr<ControlGroup> buildProgressBar();
            std::shared_ptr<ControlGroup> buildMemoryUsagePanel();
            void buildProgressBar(std::shared_ptr<ControlGroup> parent, float verticalOffset, std::string name, std::string tag);

            glm::vec2 m_prevMousePos;
            bool m_guiHidden;
            bool m_guiHidingEnabled;

            double m_timePassed;

            std::unique_ptr<Animator> m_animator;
            std::unique_ptr<RenderSystem> m_renderSystem;

            std::shared_ptr<ControlGroup> m_rootControlGroup;

            Panel* m_optionsPanel;
            Label* m_fpsLabel;
            Label* m_progressLabel;
            Panel* m_progressBar;
            Panel* m_bufferMemUsage;
            Label* m_bufferMemUsageLabel;
            Panel* m_imageMemUsage;
            Label* m_imageMemUsageLabel;
            Panel* m_stagingMemUsage;
            Label* m_stagingMemUsageLabel;

            std::vector<std::pair<std::shared_ptr<ControlGroup>, std::shared_ptr<Control>>> m_pendingControls;
        };
    }
}