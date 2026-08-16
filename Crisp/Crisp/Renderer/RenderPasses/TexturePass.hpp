#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {

template <typename ExecuteFunc>
RenderGraphResourceHandle addTexturePass(
    rg::RenderGraph& renderGraph,
    const std::string& passName,
    const VkExtent2D renderArea,
    const VkFormat textureFormat,
    const bool isSwapChainDependent,
    ExecuteFunc&& executeFunc) {
    RenderGraphResourceHandle output;
    renderGraph.addPass(
        passName,
        PassType::Rasterizer,
        [renderArea, textureFormat, isSwapChainDependent, passName, &output](rg::RenderGraph::Builder& builder) {
            output = builder.createAttachment(
                {
                    .sizePolicy = isSwapChainDependent ? SizePolicy::SwapChainRelative : SizePolicy::Absolute,
                    .width = renderArea.width,
                    .height = renderArea.height,
                    .format = textureFormat,
                    .imageUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                },
                fmt::format("{}-color", passName),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 1.0f}}});
        },
        std::forward<ExecuteFunc>(executeFunc));
    return output;
}
} // namespace crisp
