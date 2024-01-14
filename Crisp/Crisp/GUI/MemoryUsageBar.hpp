#pragma once

#include <Crisp/Gui/Panel.hpp>

namespace crisp::gui {
class Form;
class Label;
class StopWatch;

class MemoryUsageBar : public Panel {
public:
    MemoryUsageBar(Form* parentForm);
    virtual ~MemoryUsageBar();

private:
    std::unique_ptr<StopWatch> m_stopWatch;
    Label* m_bufferMemoryUsageLabel;
    Label* m_imageMemoryUsageLabel;
    Label* m_stagingMemoryUsageLabel;
};
} // namespace crisp::gui