#include <Crisp/Models/Tonemap.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {
constexpr const char* kTonemapPass = "tonemapPass";
constexpr const char* kTonemapMaterialId = "tonemap";
constexpr const char* kTonemapSamplerId = "tonemapLinearClamp";
} // namespace

void addTonemapPass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    const RenderGraphResourceHandle hdrImage) {
    resourceContext.imageCache.addSampler(kTonemapSamplerId, createLinearClampSampler(renderer.getDevice()));

    renderGraph.addPass(
        kTonemapPass,
        [hdrImage](rg::RenderGraph::Builder& builder) {
            builder.readTexture(hdrImage);

            auto& data = builder.getBlackboard().insert<TonemapPassData>();
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    // Still floating point: the curve output is in [0, 1] but quantizing here would band before
                    // the sRGB encode gets a chance to distribute the error.
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                },
                "tonemappedImage");
            builder.exportTexture(data.image);
        },
        [&renderer, &resourceContext, &renderGraph, hdrImage, material = static_cast<Material*>(nullptr)](
            const FrameContext& ctx) mutable {
            // The render graph only allocates its physical images during compile(), so the material cannot be
            // built until the pass first executes.
            if (material == nullptr) {
                VulkanPipeline* pipeline = resourceContext.pipelineCache.loadPipeline(
                    kTonemapMaterialId,
                    "Tonemap.json",
                    renderer.getShaderCache(),
                    renderer.getDevice(),
                    renderGraph.getRenderPass(kTonemapPass),
                    0);
                material = resourceContext.createMaterial(kTonemapMaterialId, pipeline);
                material->writeDescriptor(0, 0, *resourceContext.getRingBuffer(kTonemapBufferId));
                material->writeDescriptor(
                    1,
                    0,
                    renderGraph.getResourceImageView(hdrImage),
                    resourceContext.imageCache.getSampler(kTonemapSamplerId));
                renderer.getDevice().flushDescriptorUpdates();
            }

            const VkCommandBuffer cmdBufferHandle = ctx.commandEncoder.getHandle();
            material->getPipeline()->bind(cmdBufferHandle);
            material->bind(cmdBufferHandle);
            renderer.drawFullScreenQuad(cmdBufferHandle);
        });
}

} // namespace crisp
