#include <Crisp/Gui/ImGuiCameraUtils.hpp>

#include <imgui.h>

#include <Crisp/Math/GlmFormat.hpp>

namespace crisp {
void drawCameraUi(const Camera& camera, const bool isSeparateWindow) {
    if (isSeparateWindow) {
        ImGui::Begin("Camera");
    }
    ImGui::LabelText("Position", "%s", fmt::format("{}", camera.getPosition()).c_str());          // NOLINT
    ImGui::LabelText("Look", "%s", fmt::format("{}", camera.getLookDir()).c_str());               // NOLINT
    ImGui::LabelText("Up", "%s", fmt::format("{}", camera.getUpDir()).c_str());                   // NOLINT
    ImGui::LabelText("Right", "%s", fmt::format("{}", camera.getRightDir()).c_str());             // NOLINT
    ImGui::LabelText("Depth Range", "%s", fmt::format("{}", camera.getViewDepthRange()).c_str()); // NOLINT
    ImGui::LabelText("Vertical Field of View", "%.2f", camera.getVerticalFov());                  // NOLINT
    if (isSeparateWindow) {
        ImGui::End();
    }
}

void drawCameraControllerUi(const TargetCameraController& controller, const bool isSeparateWindow) {
    if (isSeparateWindow) {
        ImGui::Begin("Camera Controller");
    }
    ImGui::LabelText("Target", "%s", fmt::format("{}", controller.getTarget()).c_str());     // NOLINT
    ImGui::LabelText("Distance", "%s", fmt::format("{}", controller.getDistance()).c_str()); // NOLINT
    ImGui::LabelText("Yaw", "%s", fmt::format("{}", controller.getYaw()).c_str());           // NOLINT
    ImGui::LabelText("Pitch", "%s", fmt::format("{}", controller.getPitch()).c_str());       // NOLINT
    if (isSeparateWindow) {
        ImGui::End();
    }
}
} // namespace crisp