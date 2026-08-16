#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {

struct LiquidPassData {
    RenderGraphResourceHandle sceneColor;
    RenderGraphResourceHandle depth;
    RenderGraphResourceHandle image;
};

template <typename GeometryExecuteFunc, typename CompositeExecuteFunc>
LiquidPassData addLiquidPasses(
    rg::RenderGraph& renderGraph,
    const std::string& passName,
    const VkExtent2D renderArea,
    const VkFormat format,
    GeometryExecuteFunc&& geometryExecuteFunc,
    CompositeExecuteFunc&& compositeExecuteFunc) {
    LiquidPassData data;
    renderGraph.addPass(
        fmt::format("{}-geometry", passName),
        PassType::Rasterizer,
        [renderArea, format, passName, &data](rg::RenderGraph::Builder& builder) {
            data.sceneColor = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = renderArea.width,
                    .height = renderArea.height,
                    .format = format,
                },
                fmt::format("{}-scene-color", passName),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 0.0f}}});
            data.depth = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = renderArea.width,
                    .height = renderArea.height,
                    .format = VK_FORMAT_D32_SFLOAT,
                },
                fmt::format("{}-depth", passName),
                VkClearValue{.depthStencil{1.0f, 0}});
        },
        std::forward<GeometryExecuteFunc>(geometryExecuteFunc));

    renderGraph.addPass(
        fmt::format("{}-composite", passName),
        PassType::Rasterizer,
        [renderArea, format, passName, &data](rg::RenderGraph::Builder& builder) {
            builder.readTexture(data.sceneColor);
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = renderArea.width,
                    .height = renderArea.height,
                    .format = format,
                },
                fmt::format("{}-color", passName),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 0.0f}}});
        },
        std::forward<CompositeExecuteFunc>(compositeExecuteFunc));
    return data;
}
} // namespace crisp
