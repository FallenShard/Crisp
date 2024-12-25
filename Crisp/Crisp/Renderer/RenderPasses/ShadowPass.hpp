#pragma once

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphHandles.hpp>
#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

namespace crisp {

std::unique_ptr<VulkanRenderPass> createShadowMapPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, uint32_t shadowMapSize, uint32_t cascadeIndex = 0);

std::unique_ptr<VulkanRenderPass> createVarianceShadowMappingPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, uint32_t shadowMapSize);

inline constexpr uint32_t kDefaultCascadeCount = 4;
inline constexpr std::array<const char*, kDefaultCascadeCount> kCsmPasses = {
    "csmPass0",
    "csmPass1",
    "csmPass2",
    "csmPass3",
};

struct CascadedShadowMapData {
    std::array<RenderGraphResourceHandle, kDefaultCascadeCount> cascades;
};

template <typename ExecuteFunc>
void addCascadedShadowMapPasses(rg::RenderGraph& renderGraph, const uint32_t shadowMapSize, const ExecuteFunc& func) {
    for (uint32_t i = 0; i < kCsmPasses.size(); ++i) {
        renderGraph.addPass(
            kCsmPasses[i],
            [i, shadowMapSize](rg::RenderGraph::Builder& builder) {
                auto& data =
                    i == 0 ? builder.getBlackboard().insert<CascadedShadowMapData>()
                           : builder.getBlackboard().get<CascadedShadowMapData>();

                data.cascades[i] = builder.createAttachment(
                    {
                        .sizePolicy = SizePolicy::Absolute,
                        .width = shadowMapSize,
                        .height = shadowMapSize,
                        .format = VK_FORMAT_D32_SFLOAT,
                    },
                    fmt::format("cascaded-shadow-map-{}", i),
                    VkClearValue{.depthStencil{1.0f, 0}});
            },
            [func, i](const RenderPassExecutionContext& ctx) { func(ctx, i); });
    }
}

} // namespace crisp
