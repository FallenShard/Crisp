#pragma once

#include <filesystem>
#include <span>

#include <imgui.h>

#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

namespace crisp::gui {

void initImGui(
    GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    const VulkanDevice& device,
    uint32_t swapChainImageCount,
    VkRenderPass renderPass,
    const std::optional<std::filesystem::path>& fontPath);
void shutdownImGui(VkDevice device);
void prepareImGui();
void renderImGui(VkCommandBuffer cmdBuffer);

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
