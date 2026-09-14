#include <Crisp/Models/Tonemap.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {
constexpr const char* kTonemapPass = "tonemapPass";
constexpr const char* kTonemapMaterialId = "tonemap";
constexpr const char* kTonemapSamplerId = "tonemapLinearClamp";
constexpr const char* kTonemapComputePass = "tonemapComputePass";
} // namespace

void addTonemapPass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    const RenderGraphResourceHandle hdrImage,
    const VkImageUsageFlags extraImageUsageFlags) {
    resourceContext.imageCache.addSampler(kTonemapSamplerId, createLinearClampSampler(renderer.getDevice()));

    renderGraph.addPass(
        kTonemapPass,
        PassType::Rasterizer,
        [hdrImage, extraImageUsageFlags](rg::RenderGraph::Builder& builder) {
            builder.readTexture(hdrImage);

            auto& data = builder.getBlackboard().insert<TonemapPassData>();
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    // Still floating point: the curve output is in [0, 1] but quantizing here would band before
                    // the sRGB encode gets a chance to distribute the error.
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                    .imageUsageFlags = extraImageUsageFlags,
                },
                "tonemappedImage");
            builder.exportTexture(data.image);
        },
        [&renderer,
         &resourceContext,
         &renderGraph,
         hdrImage,
         material = static_cast<Material*>(nullptr),
         boundHdrView = VkImageView{VK_NULL_HANDLE}](const FrameContext& ctx) mutable {
            // The render graph only allocates its physical images during compile(), so the material cannot be
            // built until the pass first executes.
            if (material == nullptr) {
                VulkanPipeline* pipeline = resourceContext.pipelineCache.loadPipeline(
                    kTonemapMaterialId,
                    "Tonemap.json",
                    renderer.getDevice(),
                    {renderGraph.getRasterizationPassDescriptor(kTonemapPass)});
                material = resourceContext.createMaterial(kTonemapMaterialId, pipeline);
                material->writeDescriptor(0, 0, *resourceContext.getRingBuffer(kTonemapBufferId));
            }

            // A resize recompiles the graph and hands out a new view, leaving the bound one destroyed.
            const VulkanImageView& hdrView = renderGraph.getResourceImageView(hdrImage);
            if (hdrView.getHandle() != boundHdrView) {
                boundHdrView = hdrView.getHandle();
                material->writeDescriptor(1, 0, hdrView, resourceContext.imageCache.getSampler(kTonemapSamplerId));
                renderer.getDevice().flushDescriptorUpdates();
            }

            ctx.commandEncoder.bindPipeline(*material->getPipeline());
            ctx.commandEncoder.bindDescriptorSets(material->getDescriptorSetBinding());
            renderer.drawFullScreenQuad(ctx.commandEncoder);
        });
}

void addTonemapComputePass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    const RenderGraphResourceHandle hdrImage,
    const VkImageUsageFlags extraImageUsageFlags) {
    resourceContext.imageCache.addSampler(kTonemapSamplerId, createLinearClampSampler(renderer.getDevice()));

    renderGraph.addPass(
        kTonemapComputePass,
        PassType::Compute,
        [hdrImage, extraImageUsageFlags](rg::RenderGraph::Builder& builder) {
            builder.readTexture(hdrImage);

            auto& data = builder.getBlackboard().insert<TonemapPassData>();
            data.image = builder.createStorageImage(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                    .imageUsageFlags = extraImageUsageFlags,
                },
                "tonemappedImage");
            builder.exportTexture(data.image);
        },
        [&renderer,
         &resourceContext,
         &renderGraph,
         hdrImage,
         pipeline = std::shared_ptr<VulkanPipeline>{},
         material = std::shared_ptr<Material>{},
         boundHdrView = VkImageView{VK_NULL_HANDLE}](const FrameContext& ctx) mutable {
            constexpr VkExtent3D kWorkGroupSize{8, 8, 1};
            if (pipeline == nullptr) {
                pipeline = createComputePipeline(
                    renderer.getDevice(),
                    renderer.getAssetPaths().getShaderSpvPath("PostProcess/tonemap.comp"),
                    kWorkGroupSize);
                material = std::make_shared<Material>(pipeline.get());
                material->writeDescriptor(0, 0, *resourceContext.getRingBuffer(kTonemapBufferId));
            }

            const VulkanImageView& hdrView = renderGraph.getResourceImageView(hdrImage);
            if (hdrView.getHandle() != boundHdrView) {
                boundHdrView = hdrView.getHandle();
                const auto& outputView =
                    renderGraph.getResourceImageView(renderGraph.getBlackboard().get<TonemapPassData>().image);
                material->writeDescriptor(1, 0, hdrView, resourceContext.imageCache.getSampler(kTonemapSamplerId));
                material->writeDescriptor(1, 1, outputView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
                renderer.getDevice().flushDescriptorUpdates();
            }

            ctx.commandEncoder.bindPipeline(*pipeline);
            ctx.commandEncoder.bindDescriptorSets(material->getDescriptorSetBinding());
            const VkExtent2D extent = renderer.getSwapChainExtent();
            ctx.commandEncoder.dispatchCompute(
                computeWorkGroupCount(glm::uvec3(extent.width, extent.height, 1), kWorkGroupSize));
        });
}

} // namespace crisp
