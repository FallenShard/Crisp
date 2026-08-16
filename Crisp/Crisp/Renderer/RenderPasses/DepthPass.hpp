#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {

template <typename ExecuteFunc>
RenderGraphResourceHandle addDepthPass(
    rg::RenderGraph& renderGraph, const std::string& passName, ExecuteFunc&& executeFunc) {
    RenderGraphResourceHandle output;
    renderGraph.addPass(
        passName,
        PassType::Rasterizer,
        [passName, &output](rg::RenderGraph::Builder& builder) {
            output = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D32_SFLOAT,
                },
                fmt::format("{}-depth", passName),
                VkClearValue{.depthStencil{1.0f, 0}});
        },
        std::forward<ExecuteFunc>(executeFunc));
    return output;
}
} // namespace crisp
