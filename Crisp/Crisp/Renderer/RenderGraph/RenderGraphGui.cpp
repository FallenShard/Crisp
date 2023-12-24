#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>

#include <imgui.h>

namespace crisp {

void drawGui(const rg::RenderGraph& renderGraph) {

    for (const auto& pass : renderGraph.getPasses()) {
        if (ImGui::TreeNode(pass.name.c_str())) {
            for (const auto& res : pass.inputs) {
                ImGui::Text("IN > %s", renderGraph.getResources().at(res.id).name.c_str()); // NOLINT
            }

            for (const auto& res : pass.outputs) {
                ImGui::Text("OUT > %s", renderGraph.getResources().at(res.id).name.c_str()); // NOLINT
            }
            ImGui::TreePop();
        }
    }
}

} // namespace crisp