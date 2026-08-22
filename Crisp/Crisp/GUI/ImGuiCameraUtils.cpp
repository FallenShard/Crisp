#include <Crisp/Gui/ImGuiCameraUtils.hpp>

#include <cmath>

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

void drawCameraControllerUi(TargetCameraController& controller, const bool isSeparateWindow) {
    if (isSeparateWindow) {
        ImGui::Begin("Camera Controller");
    }
    ImGui::TextDisabled("WASD: fly  |  RMB: look  |  Wheel: dolly");
    ImGui::TextDisabled("Ctrl+LMB: orbit  |  Ctrl+RMB: pan  |  Ctrl+Wheel: distance");
    ImGui::LabelText("Target", "%s", fmt::format("{}", controller.getTarget()).c_str()); // NOLINT
    float orbitDistance = controller.getDistance();
    if (ImGui::DragFloat("Orbit Distance", &orbitDistance, 0.05f, 0.01f, 100.0f, "%.2f")) {
        controller.setDistance(orbitDistance);
    }
    ImGui::LabelText("Yaw", "%s", fmt::format("{}", controller.getYaw()).c_str());     // NOLINT
    ImGui::LabelText("Pitch", "%s", fmt::format("{}", controller.getPitch()).c_str()); // NOLINT
    if (isSeparateWindow) {
        ImGui::End();
    }
}

void drawCameraPivot(const TargetCameraController& controller) {
    if (!controller.isOrbiting()) {
        return;
    }

    const Camera& camera = controller.getCamera();
    const glm::vec4 clipPosition =
        camera.getProjectionMatrix() * camera.getViewMatrix() * glm::vec4(controller.getTarget(), 1.0f);
    if (clipPosition.w <= 0.0f) {
        return;
    }

    const glm::vec2 ndc = glm::vec2(clipPosition) / clipPosition.w;
    if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 markerPosition{
        viewport->Pos.x + (ndc.x * 0.5f + 0.5f) * viewport->Size.x,
        viewport->Pos.y + (ndc.y * 0.5f + 0.5f) * viewport->Size.y};

    constexpr float kRadius = 7.0f;
    constexpr float kCrossExtent = 11.0f;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    drawList->AddCircleFilled(markerPosition, 2.0f, IM_COL32(0, 0, 0, 220));
    drawList->AddCircle(markerPosition, kRadius, IM_COL32(255, 190, 64, 255), 0, 2.0f);
    drawList->AddLine(
        {markerPosition.x - kCrossExtent, markerPosition.y},
        {markerPosition.x + kCrossExtent, markerPosition.y},
        IM_COL32(255, 190, 64, 220),
        1.5f);
    drawList->AddLine(
        {markerPosition.x, markerPosition.y - kCrossExtent},
        {markerPosition.x, markerPosition.y + kCrossExtent},
        IM_COL32(255, 190, 64, 220),
        1.5f);
}
} // namespace crisp
