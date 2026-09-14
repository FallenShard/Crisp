#include <Crisp/Lights/LightCullingPass.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>

namespace crisp {
namespace {
constexpr const char* kLightCullingPass = "lightCullingPass";

constexpr VkExtent3D kWorkGroupSize{64, 1, 1};
} // namespace

void addLightCullingPass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    LightSystem& lightSystem,
    const std::string& viewBufferId) {
    renderGraph.addPass(
        kLightCullingPass,
        PassType::Compute,
        [](rg::RenderGraph::Builder&) {},
        [&renderer,
         &resourceContext,
         &lightSystem,
         viewBufferId,
         pipeline = std::shared_ptr<VulkanPipeline>{},
         material = std::shared_ptr<Material>{},
         boundAabbBuffer = VkBuffer{VK_NULL_HANDLE}](const FrameContext& ctx) mutable {
            const auto& clustering = lightSystem.getLightClustering();
            const uint32_t lightCount = lightSystem.getPointLightCount();
            if (lightCount == 0) {
                return;
            }

            if (pipeline == nullptr) {
                pipeline = createComputePipeline(
                    renderer.getDevice(),
                    renderer.getAssetPaths().getShaderSpvPath("Lighting/light-culling.comp"),
                    kWorkGroupSize);
                material = std::make_shared<Material>(pipeline.get());
            }

            const VkBuffer aabbBuffer = clustering.m_clusterAabbBuffer->getHandle();
            if (aabbBuffer != boundAabbBuffer) {
                boundAabbBuffer = aabbBuffer;
                material->writeDescriptor(0, 0, *clustering.m_clusterAabbBuffer);
                material->writeDescriptor(0, 1, *clustering.m_lightIndexCountBuffer);
                material->writeDescriptor(0, 2, *lightSystem.getPointLightBuffer());
                material->writeDescriptor(0, 3, *resourceContext.getRingBuffer(viewBufferId));
                material->writeDescriptor(0, 4, *clustering.m_lightIndexListBuffer);
                material->writeDescriptor(0, 5, *clustering.m_lightGridBuffer);
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

            const glm::ivec3 gridSize{clustering.m_clusterGridSize};
            ctx.commandEncoder.dispatchCompute({
                static_cast<uint32_t>(gridSize.x),
                static_cast<uint32_t>(gridSize.y),
                static_cast<uint32_t>(gridSize.z),
            });

            ctx.commandEncoder.insertBarrier(kComputeStorageWrite >> kFragmentRead);
        });
}

} // namespace crisp
