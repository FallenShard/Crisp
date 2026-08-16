#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {

template <typename ExecuteFunc>
RenderGraphResourceHandle addBlurPass(
    rg::RenderGraph& renderGraph,
    const std::string& passName,
    const RenderGraphResourceHandle input,
    const VkFormat format,
    const VkExtent2D renderArea,
    const bool isSwapChainDependent,
    ExecuteFunc&& executeFunc) {
    RenderGraphResourceHandle output;
    renderGraph.addPass(
        passName,
        PassType::Rasterizer,
        [input, format, renderArea, isSwapChainDependent, passName, &output](rg::RenderGraph::Builder& builder) {
            builder.readTexture(input);
            output = builder.createAttachment(
                {
                    .sizePolicy = isSwapChainDependent ? SizePolicy::SwapChainRelative : SizePolicy::Absolute,
                    .width = renderArea.width,
                    .height = renderArea.height,
                    .format = format,
                },
                fmt::format("{}-color", passName),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 0.0f}}});
        },
        std::forward<ExecuteFunc>(executeFunc));
    return output;
}

} // namespace crisp
