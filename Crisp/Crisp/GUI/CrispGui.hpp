#pragma once

#include <memory>
#include <string>

namespace crisp
{
class Application;
class Scene;
} // namespace crisp

namespace crisp::gui
{
class Control;
class ControlGroup;
class Form;

class CrispGui
{
public:
    void setupInputCallbacks(Form* form, Application* app, Scene* scene);

    std::unique_ptr<Control> buildCameraInfoPanel(Form* form);

private:
    void buildVectorDisplayInfo(Form* form, ControlGroup& parent, std::string labelName, std::string&& dataLabelName,
        float verticalOffset);
};
} // namespace crisp::gui