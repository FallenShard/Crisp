#pragma once

#include <Crisp/Core/Window.hpp>
#include <Crisp/Renderer/Renderer.hpp>

namespace crisp::gui
{
void initImGui(GLFWwindow* window, Renderer& renderer);
void shutdownImGui(Renderer& renderer);
void prepareImGuiFrame();
void renderImGuiFrame(Renderer& renderer);
} // namespace crisp::gui
