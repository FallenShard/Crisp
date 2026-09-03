#include <Crisp/Models/Ssao.hpp>

#include <array>
#include <random>

#include <Crisp/Math/Warp.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {
constexpr const char* kSsaoPass = "ssaoPass";
constexpr const char* kSsaoMaterialId = "ssao";
constexpr const char* kSsaoNoiseImageId = "ssaoNoise";
constexpr const char* kSsaoSampleBufferId = "ssaoSamples";
constexpr const char* kSsaoNoiseSamplerId = "ssaoLinearRepeat";
constexpr const char* kSsaoInputSamplerId = "ssaoNearestClamp";

constexpr uint32_t kNoiseExtent = 4;

using SsaoSampleArray = std::array<glm::vec4, kSsaoSampleCount>;

std::unique_ptr<VulkanImage> createRandomRotationImage(Renderer& renderer) {
    std::default_random_engine rng(43);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::vector<glm::vec4> noiseTexData;
    noiseTexData.reserve(kNoiseExtent * kNoiseExtent);
    for (uint32_t i = 0; i < kNoiseExtent * kNoiseExtent; ++i) {
        noiseTexData.emplace_back(
            glm::normalize(glm::vec3{dist(rng) * 2.0f - 1.0f, dist(rng) * 2.0f - 1.0f, 0.0f}), 1.0f);
    }

    VkImageCreateInfo noiseTexInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    noiseTexInfo.flags = 0;
    noiseTexInfo.imageType = VK_IMAGE_TYPE_2D;
    noiseTexInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    noiseTexInfo.extent = {kNoiseExtent, kNoiseExtent, 1u};
    noiseTexInfo.mipLevels = 1;
    noiseTexInfo.arrayLayers = 1;
    noiseTexInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    noiseTexInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    noiseTexInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    noiseTexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    noiseTexInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return createVulkanImage(renderer, noiseTexData.size() * sizeof(glm::vec4), noiseTexData.data(), noiseTexInfo);
}

SsaoSampleArray createHemisphereSamples() {
    std::default_random_engine randomEngine(42);
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    SsaoSampleArray samples{};
    for (auto& sample : samples) {
        const float x = distribution(randomEngine);
        const float y = distribution(randomEngine);
        const float r = distribution(randomEngine);
        sample = glm::vec4(warp::cubeToUniformHemisphereVolume({x, y, r}), 1.0f);
    }
    return samples;
}
} // namespace

void addSsaoPass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    const RenderGraphResourceHandle depthNormalImage,
    const std::string& viewBufferId,
    const SsaoParameters& parameters) {
    auto& imageCache = resourceContext.imageCache;
    imageCache.addImage(kSsaoNoiseImageId, createRandomRotationImage(renderer));
    imageCache.addImageView(
        kSsaoNoiseImageId,
        createView(renderer.getDevice(), imageCache.getImage(kSsaoNoiseImageId), VK_IMAGE_VIEW_TYPE_2D));
    imageCache.addSampler(kSsaoNoiseSamplerId, createLinearRepeatSampler(renderer.getDevice()));

    // Nearest on the input: interpolating across a depth discontinuity invents a surface that never existed.
    imageCache.addSampler(kSsaoInputSamplerId, createNearestClampSampler(renderer.getDevice()));

    const SsaoSampleArray samples = createHemisphereSamples();
    resourceContext.createRingBufferFromStruct(kSsaoSampleBufferId, samples, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT);

    renderGraph.addPass(
        kSsaoPass,
        PassType::Rasterizer,
        [depthNormalImage](rg::RenderGraph::Builder& builder) {
            builder.readTexture(depthNormalImage);

            auto& data = builder.getBlackboard().insert<SsaoPassData>();
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                "ssaoImage");
        },
        [&renderer,
         &resourceContext,
         &renderGraph,
         depthNormalImage,
         viewBufferId,
         parameters,
         material = static_cast<Material*>(nullptr),
         boundInputView = VkImageView{VK_NULL_HANDLE}](const FrameContext& ctx) mutable {
            if (material == nullptr) {
                VulkanPipeline* pipeline = resourceContext.pipelineCache.loadPipeline(
                    kSsaoMaterialId,
                    "Ssao.json",
                    renderer.getDevice(),
                    {renderGraph.getRasterizationPassDescriptor(kSsaoPass)});
                material = resourceContext.createMaterial(kSsaoMaterialId, pipeline);
                material->writeDescriptor(0, 1, *resourceContext.getRingBuffer(viewBufferId));
                material->writeDescriptor(0, 2, *resourceContext.getRingBuffer(kSsaoSampleBufferId));
                material->writeDescriptor(
                    0,
                    3,
                    resourceContext.imageCache.getImageView(kSsaoNoiseImageId),
                    resourceContext.imageCache.getSampler(kSsaoNoiseSamplerId));
            }

            const VulkanImageView& inputView = renderGraph.getResourceImageView(depthNormalImage);
            if (inputView.getHandle() != boundInputView) {
                boundInputView = inputView.getHandle();
                material->writeDescriptor(0, 0, inputView, resourceContext.imageCache.getSampler(kSsaoInputSamplerId));
                renderer.getDevice().flushDescriptorUpdates();
            }

            ctx.commandEncoder.bindPipeline(*material->getPipeline());
            ctx.commandEncoder.bindDescriptorSets(material->getDescriptorSetBinding());
            ctx.commandEncoder.setPushConstants(
                *material->getPipeline()->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, parameters);
            renderer.drawFullScreenQuad(ctx.commandEncoder);
        });
}

} // namespace crisp
