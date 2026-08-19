#pragma once

#include <Crisp/Models/Atmosphere.hpp>

namespace crisp {

// Not every scene owns the whole atmosphere. One that only consumes the LUTs has no full screen march to debug
// and no sun disk to draw, so those sections would offer controls that do nothing.
struct AtmosphereGuiOptions {
    bool showDebugViewMode{true};
    bool showSunDisk{true};
    bool showRayMarchingDebug{true};
};

// Draws the contents of an atmosphere panel into the current ImGui window. The caller owns the window, so a
// scene can either give it one of its own or fold it into a larger panel.
void drawAtmosphereGuiContents(
    AtmosphereSettings& settings, AtmosphereParameters& params, const AtmosphereGuiOptions& options = {});

} // namespace crisp
