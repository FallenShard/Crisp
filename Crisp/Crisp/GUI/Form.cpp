#include <Crisp/Gui/Form.hpp>

#include <iostream>

#include <Crisp/Core/Logger.hpp>

#include <Crisp/Animation/PropertyAnimation.hpp>
#include <Crisp/Gui/ControlGroup.hpp>
#include <Crisp/Gui/RenderSystem.hpp>
#include <Crisp/Gui/StopWatch.hpp>

#include <Crisp/Gui/Panel.hpp>

namespace {
auto logger = spdlog::stderr_color_mt("Form");
}

namespace crisp::gui {
Form::Form(std::unique_ptr<RenderSystem> renderSystem)
    : m_renderSystem(std::move(renderSystem))
    , m_animator(std::make_unique<Animator>())
    , m_rootControlGroup(std::make_unique<ControlGroup>(this))
    , m_focusedControl(nullptr) {
    m_rootControlGroup->setId("root");
    m_rootControlGroup->setDepthOffset(-32.0f);
    m_rootControlGroup->setSizeHint(m_renderSystem->getScreenSize());
}

Form::~Form() {}

RenderSystem* Form::getRenderSystem() {
    return m_renderSystem.get();
}

Animator* Form::getAnimator() {
    return m_animator.get();
}

void Form::addStopWatch(StopWatch* stopWatch) {
    m_stopWatches.insert(stopWatch);
}

void Form::removeStopWatch(StopWatch* stopWatch) {
    m_stopWatches.erase(stopWatch);
}

void Form::postGuiUpdate(std::function<void()> guiUpdateCallback) {
    m_guiUpdates.push_back(guiUpdateCallback);
}

void Form::add(std::unique_ptr<Control> control, bool /*useFadeInAnimation*/) {
    // TODO: Rethink animations for GUI tree manipulations
    // m_rootControlGroup->addControl(useFadeInAnimation ? fadeIn(std::move(control)) : std::move(control));
    m_rootControlGroup->addControl(std::move(control));
}

void Form::remove(std::string controlId, float /*duration*/) {
    auto control = m_rootControlGroup->getControlById(controlId);
    if (!control) {
        logger->warn("Attempt to delete a non-existing control with id: {}", controlId);
        return;
    }

    // TODO: Rethink animations for GUI tree manipulations
    auto parent = static_cast<ControlGroup*>(control->getParent());
    if (parent) {
        parent->removeControl(controlId);
    }
    /*auto colorAnim = std::make_shared<PropertyAnimation<float>>(duration, 1.0f, 0.0f, [control](const auto& t)
    {
        control->setOpacity(t);
    });

    colorAnim->finished.subscribe([this, control]()
    {
        m_rootControlGroup->removeControl(control->getId());
    });
    m_animator->add(colorAnim);*/
}

void Form::processGuiUpdates() {
    if (!m_guiUpdates.empty()) {
        for (auto& guiUpdate : m_guiUpdates) {
            guiUpdate();
        }

        m_guiUpdates.clear();

        printGuiTree();
    }
}

void Form::update(double dt) {
    for (auto& stopWatch : m_stopWatches) {
        stopWatch->accumulate(dt);
    }

    m_animator->update(dt);
}

void Form::draw() {
    validateControls();

    m_rootControlGroup->draw(*m_renderSystem);
    m_renderSystem->submitDrawCommands();
}

void Form::printGuiTree() {
    m_rootControlGroup->printDebugId();
}

void Form::visit(std::function<void(Control*)> func) {
    m_rootControlGroup->visit(func);
}

void Form::removeFromValidationList(Control* control) {
    m_validationSet.erase(control);
}

void Form::addToValidationList(Control* control) {
    m_validationSet.insert(control);
}

void Form::resize(int width, int height) {
    m_renderSystem->resize(width, height);
    m_rootControlGroup->setSizeHint({width, height});
}

void Form::setFocusedControl(Control* control) {
    m_focusedControl = control;
    if (m_focusedControl) {
        logger->debug("Focused control is: {}", control->getId());
    } else {
        logger->debug("Focus cleared.");
    }
}

void Form::onMouseEntered(double mouseX, double mouseY) {
    m_rootControlGroup->onMouseEntered(static_cast<float>(mouseX), static_cast<float>(mouseY));
}

void Form::onMouseExited(double mouseX, double mouseY) {
    m_rootControlGroup->onMouseExited(static_cast<float>(mouseX), static_cast<float>(mouseY));
}

void Form::onMouseMoved(double x, double y) {
    if (m_focusedControl) {
        m_focusedControl->onMouseMoved(static_cast<float>(x), static_cast<float>(y));
    } else {
        m_rootControlGroup->onMouseMoved(static_cast<float>(x), static_cast<float>(y));
    }
}

void Form::onMousePressed(const MouseEventArgs& mouseEventArgs) {
    if (m_focusedControl && m_focusedControl->getInteractionBounds().contains(mouseEventArgs.x, mouseEventArgs.y)) {
        m_focusedControl->onMousePressed(static_cast<float>(mouseEventArgs.x), static_cast<float>(mouseEventArgs.y));
    } else {
        m_rootControlGroup->onMousePressed(static_cast<float>(mouseEventArgs.x), static_cast<float>(mouseEventArgs.y));
    }
}

void Form::onMouseReleased(const MouseEventArgs& mouseEventArgs) {
    if (m_focusedControl) {
        Control* control = m_focusedControl;
        while (control &&
               !control->onMouseReleased(static_cast<float>(mouseEventArgs.x), static_cast<float>(mouseEventArgs.y))) {
            control = control->getParent();
        }
    }
}

void Form::validateControls() {
    while (!m_validationSet.empty()) {
        Control* control = *m_validationSet.begin();
        m_validationSet.erase(*m_validationSet.begin());

        if (control && control->needsValidation()) {
            Control* iter = control->getParent();
            while (iter) {
                bool resizedWithChildren =
                    iter->getHorizontalSizingPolicy() == SizingPolicy::WrapContent ||
                    iter->getVerticalSizingPolicy() == SizingPolicy::WrapContent;
                if (iter->needsValidation() || resizedWithChildren) {
                    control = iter;
                }

                iter = iter->getParent();
            }

            control->validateAndClearFlags();
        }
    }
}

std::unique_ptr<Control> Form::fadeIn(std::unique_ptr<Control> control, float /*duration*/) {
    auto anim = std::make_shared<PropertyAnimation<float>>(0.3, 0.0f, 1.0f, [this, con = control.get()](const auto& t) {
        con->setOpacity(t);
    });
    m_animator->add(anim);

    return control;
}
} // namespace crisp::gui