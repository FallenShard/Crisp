#pragma once

#include <memory>
#include <string>
#include <vector>
#include <set>

#include "Math/Headers.hpp"
#include "Animation/Animator.hpp"
#include "GUI/ControlGroup.hpp"

namespace crisp::gui
{
    class RenderSystem;
    class StopWatch;
    class ControlGroup;

    class Form
    {
    public:
        Form(std::unique_ptr<RenderSystem> renderSystem);
        ~Form();

        Form(const Form& other) = delete;
        Form& operator=(const Form& other) = delete;
        Form& operator=(Form&& other) = delete;

        RenderSystem* getRenderSystem();
        Animator*     getAnimator();

        void addStopWatch(StopWatch* stopWatch);
        void removeStopWatch(StopWatch* stopWatch);

        void postGuiUpdate(std::function<void()>&& guiUpdateCallback);
        void add(std::shared_ptr<Control> control, bool useFadeInAnimation = true);
        void remove(std::string controlId, float duration = 1.0f);

        template <typename T>
        T* getControlById(std::string id) { return m_rootControlGroup->getTypedControlById<T>(id); }

        void resize(int width, int height);

        void setFocusedControl(Control* control);
        void onMouseEntered(double mouseX, double mouseY);
        void onMouseExited(double mouseX, double mouseY);
        void onMouseMoved(double mouseX, double mouseY);
        void onMousePressed(int button, int mods, double mouseX, double mouseY);
        void onMouseReleased(int button, int mods, double mouseX, double mouseY);

        void update(double dt);
        void draw();

    private:
        std::shared_ptr<Control> fadeIn(std::shared_ptr<Control> controlGroup, float duration = 1.0f);

        std::unique_ptr<RenderSystem>      m_renderSystem;
        std::unique_ptr<Animator>          m_animator;

        std::vector<std::function<void()>> m_guiUpdates;
        std::set<StopWatch*>               m_stopWatches;

        std::shared_ptr<ControlGroup>      m_rootControlGroup;

        Control* m_focusedControl;
    };
}