#pragma once

#include <memory>
#include <string>

namespace crisp
{
    class Application;
    class Scene;

    namespace gui
    {
        class Control;
        class ControlGroup;
        class Form;

        class CrispGui
        {
        public:
            void setupInputCallbacks(Form* form, Application* app, Scene* scene);

            std::shared_ptr<Control> buildCameraInfoPanel(Form* form);

        private:
            void buildVectorDisplayInfo(Form* form, std::shared_ptr<ControlGroup> parent, std::string labelName, std::string dataLabelName, float verticalOffset);
        };
    }
}