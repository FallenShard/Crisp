#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphHandles.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>
#include <Crisp/Vulkan/VulkanRenderPassBuilder.hpp>

namespace crisp {
std::unique_ptr<VulkanRenderPass> createForwardLightingPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, VkExtent2D renderArea);

inline constexpr const char* kForwardLightingPass = "forwardLightingPass";

struct ForwardLightingPassData {
    RenderGraphResourceHandle hdrImage;
};

template <typename Func>
void addForwardLightingPass(rg::RenderGraph& renderGraph, const Func& func) {
    renderGraph.addPass(
        kForwardLightingPass,
        [](rg::RenderGraph::Builder& builder) {
            const auto& csmData = builder.getBlackboard().get<CascadedShadowMapData>();
            for (const auto& shadowMap : csmData.cascades) {
                builder.readTexture(shadowMap);
            }

            auto& data = builder.getBlackboard().insert<ForwardLightingPassData>();
            data.hdrImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-color", kForwardLightingPass),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 1.0f}}});
            builder.exportTexture(data.hdrImage);

            builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D24_UNORM_S8_UINT,
                },
                fmt::format("{}-depth", kForwardLightingPass),
                VkClearValue{.depthStencil{0.0f, 0}});
        },
        func);
}

} // namespace crisp
