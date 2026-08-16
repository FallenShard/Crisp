#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {

template <typename ExecuteFunc>
RenderGraphResourceHandle addCubeMapPass(
    rg::RenderGraph& renderGraph,
    const std::string& passName,
    const VkExtent2D renderArea,
    const VkFormat format,
    ExecuteFunc&& executeFunc) {
    RenderGraphResourceHandle output;
    renderGraph.addPass(
        passName,
        [renderArea, format, passName, &output](rg::RenderGraph::Builder& builder) {
            output = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = renderArea.width,
                    .height = renderArea.height,
                    .format = format,
                    .layerCount = 6,
                    .createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                },
                fmt::format("{}-color", passName),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 1.0f}}});
        },
        std::forward<ExecuteFunc>(executeFunc));
    return output;
}

} // namespace crisp
