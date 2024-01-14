#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <Crisp/Animation/Animator.hpp>
#include <Crisp/Core/Mouse.hpp>
#include <Crisp/Gui/ControlGroup.hpp>
#include <Crisp/Math/Headers.hpp>

namespace crisp::gui {
class RenderSystem;
class StopWatch;
class ControlGroup;

class Form {
public:
    Form(std::unique_ptr<RenderSystem> renderSystem);
    ~Form();

    Form(const Form& other) = delete;
    Form& operator=(const Form& other) = delete;

    RenderSystem* getRenderSystem();
    Animator* getAnimator();

    void addStopWatch(StopWatch* stopWatch);
    void removeStopWatch(StopWatch* stopWatch);

    void postGuiUpdate(std::function<void()> guiUpdateCallback);
    void add(std::unique_ptr<Control> control, bool useFadeInAnimation = true);
    void remove(std::string controlId, float duration = 1.0f);

    template <typename T>
    T* getControlById(std::string id) {
        return m_rootControlGroup->getTypedControlById<T>(id);
    }

    void resize(int width, int height);

    void setFocusedControl(Control* control);
    void onMouseEntered(double mouseX, double mouseY);
    void onMouseExited(double mouseX, double mouseY);
    void onMouseMoved(double mouseX, double mouseY);
    void onMousePressed(const MouseEventArgs& mouseEventArgs);
    void onMouseReleased(const MouseEventArgs& mouseEventArgs);

    void processGuiUpdates();
    void update(double dt);
    void draw();

    void printGuiTree();
    void visit(std::function<void(Control*)> func);

    void addToValidationList(Control* control);
    void removeFromValidationList(Control* control);

private:
    void validateControls();

    std::unique_ptr<Control> fadeIn(std::unique_ptr<Control> controlGroup, float duration = 1.0f);

    std::unique_ptr<RenderSystem> m_renderSystem;
    std::unique_ptr<Animator> m_animator;
    std::unordered_set<Control*> m_validationSet;

    std::vector<std::function<void()>> m_guiUpdates;
    std::unordered_set<StopWatch*> m_stopWatches;

    std::unique_ptr<ControlGroup> m_rootControlGroup;

    Control* m_focusedControl;
};
} // namespace crisp::gui