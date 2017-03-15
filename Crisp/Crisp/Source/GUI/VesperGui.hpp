#pragma once

#include <memory>

namespace crisp
{
    class Application;

    namespace gui
    {
        class Control;
        class Form;

        class VesperGui
        {
        public:
            void setupInputCallbacks(Form* form, Application* app);

            std::shared_ptr<Control> buildSceneOptions(Form* form);
            std::shared_ptr<Control> buildProgressBar(Form* form);
        };
    }
}