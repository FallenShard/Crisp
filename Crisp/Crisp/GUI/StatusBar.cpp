#include "StatusBar.hpp"

#include <iomanip>
#include <sstream>

#include "Form.hpp"
#include "Label.hpp"

namespace crisp::gui {
StatusBar::StatusBar(Form* parentForm)
    : Panel(parentForm) {
    setId("statusBar");
    setPosition({0, 0});
    setSizeHint({0, 20});
    setPadding({3, 3});
    setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
    setHorizontalSizingPolicy(SizingPolicy::FillParent);

    auto fpsLabel = std::make_unique<Label>(parentForm, "125.55 FPS");
    fpsLabel->setId("fpsLabel");
    fpsLabel->setAnchor(Anchor::CenterRight);
    fpsLabel->setOrigin(Origin::CenterRight);
    m_fpsLabel = fpsLabel.get();
    addControl(std::move(fpsLabel));

    auto msLabel = std::make_unique<Label>(parentForm, "12.86 ms");
    msLabel->setId("msLabel");
    msLabel->setAnchor(Anchor::CenterRight);
    msLabel->setOrigin(Origin::CenterRight);
    msLabel->setPosition({75, 0});
    m_msLabel = msLabel.get();
    addControl(std::move(msLabel));
}

StatusBar::~StatusBar() {}

void StatusBar::setFrameTimeAndFps(double frameTime, double fps) {
    std::ostringstream msLabelStream;
    msLabelStream << std::fixed << std::setprecision(2) << std::setfill('0') << frameTime * 1000.0 << " ms";
    m_msLabel->setText(msLabelStream.str());

    std::ostringstream fpsLabelStream;
    fpsLabelStream << std::fixed << std::setprecision(2) << std::setfill('0') << fps << " FPS";
    m_fpsLabel->setText(fpsLabelStream.str());
}
} // namespace crisp::gui