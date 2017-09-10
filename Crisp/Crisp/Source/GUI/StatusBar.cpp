#include "StatusBar.hpp"

#include <sstream>
#include <iomanip>

#include "Form.hpp"
#include "Label.hpp"

namespace crisp::gui
{
    StatusBar::StatusBar(Form* parentForm)
        : Panel(parentForm)
    {
        setId("statusBar");
        setPosition({ 0, 0 });
        setSizeHint({ 0, 20 });
        setPadding({ 3, 3 });
        setColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
        setHorizontalSizingPolicy(SizingPolicy::FillParent);

        auto fpslabel = std::make_shared<gui::Label>(parentForm, "125.55 FPS");
        fpslabel->setId("fpsLabel");
        fpslabel->setAnchor(Anchor::CenterRight);
        addControl(fpslabel);
        m_fpsLabel = fpslabel.get();

        auto msLabel = std::make_shared<gui::Label>(parentForm, "12.86 ms");
        msLabel->setId("msLabel");
        msLabel->setAnchor(Anchor::CenterRight);
        msLabel->setPosition({ 75, 0 });
        addControl(msLabel);
        m_msLabel = msLabel.get();
    }

    StatusBar::~StatusBar()
    {
    }

    void StatusBar::setFrameTimeAndFps(double frameTime, double fps)
    {
        std::ostringstream msLabelStream;
        msLabelStream << std::fixed << std::setprecision(2) << std::setfill('0') << frameTime << " ms";
        m_msLabel->setText(msLabelStream.str());

        std::ostringstream fpsLabelStream;
        fpsLabelStream << std::fixed << std::setprecision(2) << std::setfill('0') << fps << " FPS";
        m_fpsLabel->setText(fpsLabelStream.str());
    }
}