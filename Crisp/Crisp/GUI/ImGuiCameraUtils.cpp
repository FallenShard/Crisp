#include <Crisp/Gui/ImGuiCameraUtils.hpp>

#include <imgui.h>

#include <Crisp/Math/GlmFormat.hpp>

namespace crisp {
void drawCameraUi(const Camera& camera) {
    ImGui::Begin("Camera");
    ImGui::LabelText("Position", "%s", fmt::format("{}", camera.getPosition()).c_str());          // NOLINT
    ImGui::LabelText("Look", "%s", fmt::format("{}", camera.getLookDir()).c_str());               // NOLINT
    ImGui::LabelText("Up", "%s", fmt::format("{}", camera.getUpDir()).c_str());                   // NOLINT
    ImGui::LabelText("Right", "%s", fmt::format("{}", camera.getRightDir()).c_str());             // NOLINT
    ImGui::LabelText("Depth Range", "%s", fmt::format("{}", camera.getViewDepthRange()).c_str()); // NOLINT
    ImGui::End();
}
} // namespace crisp