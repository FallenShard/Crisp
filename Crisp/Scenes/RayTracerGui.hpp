#pragma once

#include <memory>

namespace crisp
{
    class RayTracerScene;

    namespace gui
    {
        class Control;
        class Form;

        std::unique_ptr<Control> buildSceneOptions(Form* form, RayTracerScene* scene);
        std::unique_ptr<Control> buildProgressBar(Form* form, RayTracerScene* scene);
    }
}