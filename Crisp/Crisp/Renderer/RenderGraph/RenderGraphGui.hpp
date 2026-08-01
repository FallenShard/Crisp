#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {

// Draws the render graph inspector into the current ImGui window. Selection state is retained across frames and
// is reset when a different graph is passed in.
void drawRenderGraphGui(const rg::RenderGraph& renderGraph);

} // namespace crisp
