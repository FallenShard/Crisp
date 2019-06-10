#pragma once

#include "GUI/Panel.hpp"

namespace crisp
{
    class ShadowMappingScene;
}

namespace crisp::gui
{
    class Form;

    std::unique_ptr<Panel> createShadowMappingSceneGui(Form* form, ShadowMappingScene* scene);
}