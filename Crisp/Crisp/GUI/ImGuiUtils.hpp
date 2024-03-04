#pragma once

#include <span>

#include <imgui.h>

#include <Crisp/Core/Window.hpp>
#include <Crisp/Renderer/Renderer.hpp>

namespace crisp::gui {
void initImGui(GLFWwindow* window, Renderer& renderer, const std::optional<std::string>& fontPath);
void shutdownImGui(Renderer& renderer);
void prepareImGuiFrame();
void renderImGuiFrame(Renderer& renderer);

template <typename F>
inline void drawComboBox(
    const char* name, const std::string& currentItem, const std::span<const std::string> items, const F& func) {
    if (ImGui::BeginCombo(name, currentItem.c_str())) {
        for (const auto& item : items) {
            const bool isSelected{item == currentItem};
            if (ImGui::Selectable(item.c_str(), isSelected)) {
                func(item);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}
} // namespace crisp::gui
