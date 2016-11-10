#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include <Animation/Animator.hpp>

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
        class Panel;

        class TopLevelGroup
        {
        public:
            TopLevelGroup();
            ~TopLevelGroup();

            TopLevelGroup(const TopLevelGroup& other) = delete;
            TopLevelGroup& operator=(const TopLevelGroup& other) = delete;
            TopLevelGroup& operator=(TopLevelGroup&& other) = delete;

            void update(double dt);
            void setTracerProgress(float value, float timeSpentRendering);

            void resize(int width, int height);
            void onMouseMoved(double mouseX, double mouseY);
            void onMousePressed(int button, int mods, double mouseX, double mouseY);
            void onMouseReleased(int button, int mods, double mouseX, double mouseY);

            void draw();

            void setFpsString(const std::string& fps);

            gui::Button* getButton();

        private:
            std::shared_ptr<ControlGroup> buildGui();

            glm::vec2 m_prevMousePos;
            bool m_guiHidden;
            bool m_guiHidingEnabled;

            std::unique_ptr<Animator> m_animator;
            std::unique_ptr<DrawingVisitor> m_drawingVisitor;
            std::unique_ptr<FontLoader> m_fontLoader;

            std::shared_ptr<ControlGroup> m_mainGroup;
            Label* m_label;
            Button* m_button;
            Panel* m_panel;
        };
    }
}