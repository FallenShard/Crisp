#pragma once

#include <memory>

namespace crisp
{
    class Application;
}

namespace crisp::gui
{
    class Form;
    class Panel;

    std::unique_ptr<Panel> createIntroPanel(Form* form, Application* application);
}