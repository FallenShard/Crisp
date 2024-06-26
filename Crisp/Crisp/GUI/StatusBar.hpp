#pragma once

#include <Crisp/Gui/Panel.hpp>

namespace crisp::gui {
class Form;
class Label;

class StatusBar : public Panel {
public:
    StatusBar(Form* parentForm);
    virtual ~StatusBar();

    void setFrameTimeAndFps(double frameTimeInMs, double fps);

private:
    Label* m_fpsLabel;
    Label* m_msLabel;
};
} // namespace crisp::gui