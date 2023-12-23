#include <Crisp/Gui/ImGuiCameraUtils.hpp>

#include <imgui.h>

#include <Crisp/Core/Format.hpp>
#include <Crisp/Utils/GlmFormat.hpp>

namespace crisp {
void drawCameraUi(const Camera& camera) {
    ImGui::Begin("Camera");
    ImGui::LabelText("Position", fmt::format("{}", camera.getPosition()).c_str());
    ImGui::LabelText("Look", fmt::format("{}", camera.getLookDir()).c_str());
    ImGui::LabelText("Up", fmt::format("{}", camera.getUpDir()).c_str());
    ImGui::LabelText("Right", fmt::format("{}", camera.getRightDir()).c_str());
    ImGui::LabelText("Depth Range", fmt::format("{}", camera.getViewDepthRange()).c_str());
    ImGui::End();
}
} // namespace crisp