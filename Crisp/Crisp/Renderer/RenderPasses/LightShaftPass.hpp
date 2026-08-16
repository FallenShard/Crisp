#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {

template <typename ExecuteFunc>
RenderGraphResourceHandle addLightShaftPass(
    rg::RenderGraph& renderGraph,
    const std::string& passName,
    const RenderGraphImageDescription& outputDescription,
    ExecuteFunc&& executeFunc) {
    RenderGraphResourceHandle output;
    renderGraph.addPass(
        passName,
        [outputDescription, passName, &output](rg::RenderGraph::Builder& builder) {
            output = builder.createAttachment(
                outputDescription,
                fmt::format("{}-color", passName),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 0.0f}}});
        },
        std::forward<ExecuteFunc>(executeFunc));
    return output;
}
} // namespace crisp
