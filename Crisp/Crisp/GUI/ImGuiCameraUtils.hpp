#pragma once

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Camera/TargetCameraController.hpp>

namespace crisp {

void drawCameraUi(const Camera& camera, bool isSeparateWindow = true);
void drawCameraControllerUi(const TargetCameraController& controller, bool isSeparateWindow = true);

} // namespace crisp