#include <Crisp/Gui/DebugRect.hpp>

#include <iostream>
#include <random>
#include <sstream>

#include <Crisp/Gui/Label.hpp>

namespace crisp::gui {
namespace {
std::default_random_engine engine(std::random_device{}());
std::uniform_real_distribution<float> distrib(0.1f, 0.8f);
} // namespace

DebugRect::DebugRect(Form* parentForm)
    : Panel(parentForm)
    , m_label(std::make_unique<Label>(parentForm)) {
    m_color.r = distrib(engine);
    m_color.g = distrib(engine);
    m_color.b = distrib(engine);

    m_label->setParent(this);
    m_label->setPosition({10.0f, 10.0f});
}

DebugRect::~DebugRect() {}

void DebugRect::validate() {
    Panel::validate();
    std::ostringstream str;
    auto bounds = getAbsoluteBounds();
    str << "P: " << bounds.x << ", " << bounds.y;
    str << " S: " << bounds.width << ", " << bounds.height;

    m_label->setText(str.str());
    m_label->validateAndClearFlags();
}

void DebugRect::draw(const RenderSystem& renderSystem) const {
    Panel::draw(renderSystem);
    m_label->draw(renderSystem);
}
} // namespace crisp::gui