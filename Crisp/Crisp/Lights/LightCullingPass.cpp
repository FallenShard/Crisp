#include <Crisp/Lights/LightCullingPass.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {
constexpr const char* kLightCullingPass = "lightCullingPass";
constexpr const char* kDepthSamplerId = "lightCullingDepthSampler";
} // namespace

void addLightCullingPass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    LightSystem& lightSystem,
    const RenderGraphResourceHandle depthImage,
    const std::string& viewBufferId) {
    resourceContext.imageCache.addSampler(kDepthSamplerId, createNearestClampSampler(renderer.getDevice()));

    renderGraph.addPass(
        kLightCullingPass,
        PassType::Compute,
        [depthImage](rg::RenderGraph::Builder& builder) { builder.readTexture(depthImage); },
        [&renderer,
         &resourceContext,
         &renderGraph,
         &lightSystem,
         depthImage,
         viewBufferId,
         pipeline = std::shared_ptr<VulkanPipeline>{},
         material = std::shared_ptr<Material>{},
         boundGridView = VkImageView{VK_NULL_HANDLE},
         boundDepthView = VkImageView{VK_NULL_HANDLE}](const FrameContext& ctx) mutable {
            const auto& clustering = lightSystem.getLightClustering();
            const uint32_t lightCount = lightSystem.getPointLightCount();
            if (lightCount == 0) {
                return;
            }

            if (pipeline == nullptr) {
                const VkExtent3D workGroupSize{
                    static_cast<uint32_t>(clustering.m_tileSize.x), static_cast<uint32_t>(clustering.m_tileSize.y), 1u};
                pipeline = createComputePipeline(
                    renderer.getDevice(), renderer.getAssetPaths().getShaderSpvPath("light-culling.comp"), workGroupSize);
                material = std::make_shared<Material>(pipeline.get());
            }

            const VkImageView gridView = clustering.m_lightGridView->getHandle();
            const VulkanImageView& depthView = renderGraph.getResourceImageView(depthImage);
            if (gridView != boundGridView || depthView.getHandle() != boundDepthView) {
                boundGridView = gridView;
                boundDepthView = depthView.getHandle();

                material->writeDescriptor(0, 0, *clustering.m_tilePlaneBuffer);
                material->writeDescriptor(0, 1, *clustering.m_lightIndexCountBuffer);
                material->writeDescriptor(0, 2, *lightSystem.getPointLightBuffer());
                material->writeDescriptor(0, 3, *resourceContext.getRingBuffer(viewBufferId));
                material->writeDescriptor(0, 4, *clustering.m_lightIndexListBuffer);
                material->writeDescriptor(
                    1, 0, clustering.m_lightGridView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
                material->writeDescriptor(1, 1, depthView, resourceContext.imageCache.getSampler(kDepthSamplerId));
                renderer.getDevice().flushDescriptorUpdates();
            }

            constexpr uint32_t kZero{0};
            ctx.commandEncoder.insertBarrier(kFragmentRead >> kTransferWrite);
            ctx.commandEncoder.updateBuffer(
                clustering.m_lightIndexCountBuffer->getDeviceBuffer(), std::as_bytes(std::span{&kZero, 1}));
            ctx.commandEncoder.insertBarrier(kTransferWrite >> kComputeStorageWrite);

            ctx.commandEncoder.bindPipeline(*pipeline);
            ctx.commandEncoder.bindDescriptorSets(material->getDescriptorSetBinding());
            ctx.commandEncoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, lightCount);
            ctx.commandEncoder.dispatchCompute(
                {static_cast<uint32_t>(clustering.m_gridSize.x), static_cast<uint32_t>(clustering.m_gridSize.y), 1u});

            ctx.commandEncoder.insertBarrier(kComputeStorageWrite >> kFragmentRead);
        });
}

} // namespace crisp
