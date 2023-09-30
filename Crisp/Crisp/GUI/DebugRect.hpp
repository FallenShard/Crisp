#pragma once

#include "Panel.hpp"

namespace crisp::gui {
class DebugRect : public Panel {
public:
    DebugRect(Form* parentForm);
    ~DebugRect();

    virtual void validate() override;
    virtual void draw(const RenderSystem& renderSystem) const override;

private:
    std::unique_ptr<Label> m_label;
};
} // namespace crisp::gui