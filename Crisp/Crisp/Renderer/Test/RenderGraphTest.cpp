#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <Crisp/Renderer/RenderGraphExperimental.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

#include <fstream>
#include <stack>

namespace crisp::test {
namespace {

using ::testing::SizeIs;
using namespace rg;

struct FluidSimulationData {
    RenderGraphResourceHandle positionBuffer;
};

struct DepthPrePassData {
    RenderGraphResourceHandle depthImage;
};

struct ForwardLightingData {
    RenderGraphResourceHandle hdrImage;
};

class RenderGraphTest : public VulkanTestWithSurface {
protected:
    static VulkanSwapChain createSwapChain(const TripleBuffering buffering = TripleBuffering::Enabled) {
        return {*device_, *physicalDevice_, context_->getSurface(), buffering};
    }
};

VkClearValue createDepthClearValue(const float depthValue, const uint8_t stencilValue) {
    VkClearValue v{};
    v.depthStencil = {depthValue, stencilValue};
    return v;
}

TEST(RenderGraphTest2, DISABLED_BasicUsage) {
    RenderGraph rg;
    rg.addPass(
        "fluid-pass",
        [](RenderGraph::Builder& builder) {
            auto& data = builder.getBlackboard().insert<FluidSimulationData>();
            data.positionBuffer = builder.createBuffer({}, "fluid-position-buffer");
        },
        [](const RenderPassExecutionContext&) {});

    rg.addPass(
        "depth-pre-pass",
        [](RenderGraph::Builder& builder) {
            builder.readBuffer(builder.getBlackboard().get<FluidSimulationData>().positionBuffer);

            auto& data = builder.getBlackboard().insert<DepthPrePassData>();
            data.depthImage = builder.createAttachment(
                {.sizePolicy = SizePolicy::SwapChainRelative, .format = VK_FORMAT_D24_UNORM_S8_UINT},
                "depth-buffer",
                createDepthClearValue(1.0f, 0));
        },
        [](const RenderPassExecutionContext&) {});

    RenderGraphResourceHandle output{};
    rg.addPass(
        "spectrum-pass",
        [&output](RenderGraph::Builder& builder) {
            output = builder.createStorageImage({.format = VK_FORMAT_R32G32_SFLOAT}, "spectrum-image");
        },
        [](const RenderPassExecutionContext&) {});

    for (uint32_t i = 0; i < 10; ++i) {
        rg.addPass(
            fmt::format("fft-pass-{}", i),
            [&output, i](RenderGraph::Builder& builder) {
                builder.readStorageImage(output);
                output = builder.createStorageImage(
                    {.format = VK_FORMAT_R32G32_SFLOAT}, fmt::format("fft-pass-image-{}", i));
            },
            [](const RenderPassExecutionContext&) {});
    }

    rg.addPass(
        "forward-pass",
        [&output](RenderGraph::Builder& builder) {
            builder.readAttachment(builder.getBlackboard().get<DepthPrePassData>().depthImage);
            builder.readBuffer(builder.getBlackboard().get<FluidSimulationData>().positionBuffer);
            builder.readStorageImage(output);
            auto& data = builder.getBlackboard().insert<ForwardLightingData>();
            data.hdrImage = builder.createAttachment({.format = VK_FORMAT_R32G32B32A32_SFLOAT}, "hdr-image");
        },
        [](const RenderPassExecutionContext&) {});

    RenderGraphResourceHandle modifiedHdrImage{};
    rg.addPass(
        "transparent-pass",
        [&modifiedHdrImage](RenderGraph::Builder& builder) {
            builder.readAttachment(builder.getBlackboard().get<DepthPrePassData>().depthImage);
            modifiedHdrImage = builder.writeAttachment(builder.getBlackboard().get<ForwardLightingData>().hdrImage);
        },
        [](const RenderPassExecutionContext&) {});

    RenderGraphResourceHandle bloomImage{};
    rg.addPass(
        "bloom-pass",
        [modifiedHdrImage, &bloomImage](RenderGraph::Builder& builder) {
            builder.readTexture(modifiedHdrImage);
            bloomImage = builder.createAttachment({.format = VK_FORMAT_R32G32B32A32_SFLOAT}, "bloom-image");
        },
        [](const RenderPassExecutionContext&) {});

    rg.addPass(
        "tonemapping-pass",
        [bloomImage](RenderGraph::Builder& builder) {
            builder.readTexture(bloomImage);
            builder.createAttachment({.format = VK_FORMAT_R8G8B8A8_UNORM}, "ldr-image");
        },
        [](const RenderPassExecutionContext&) {});

    rg.toGraphViz("D:/graph.dot").unwrap();

    EXPECT_THAT(rg.getPassCount(), 17);
    EXPECT_THAT(rg.getResourceCount(), 17);
}

TEST_F(RenderGraphTest, BasicUsage) {
    RenderGraph rg;

    rg.addPass(
        "forward-pass",
        [](RenderGraph::Builder& builder) {
            builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D24_UNORM_S8_UINT,
                },
                "depth-buffer",
                createDepthClearValue(1.0f, 0));
            auto& data = builder.getBlackboard().insert<ForwardLightingData>();
            data.hdrImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                "hdr-image");
        },
        [](const RenderPassExecutionContext&) {});

    RenderGraphResourceHandle bloomImage{};
    rg.addPass(
        "bloom-pass",
        [&bloomImage](RenderGraph::Builder& builder) {
            const auto& forwardData = builder.getBlackboard().get<ForwardLightingData>();
            builder.readTexture(forwardData.hdrImage);
            bloomImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                "bloom-image");
        },
        [](const RenderPassExecutionContext&) {});

    constexpr VkExtent2D kSwapChainExtent{1600, 900};
    {
        ScopeCommandExecutor executor(*device_);
        rg.compile(*device_, kSwapChainExtent, executor.cmdBuffer.getHandle());
    }

    {
        ScopeCommandExecutor executor(*device_);
        rg.execute(executor.cmdBuffer.getHandle());
    }

    rg.toGraphViz("D:/graph.dot").unwrap();

    EXPECT_THAT(rg.getPassCount(), 2);
    EXPECT_THAT(rg.getResourceCount(), 3);
}

TEST(RenderGraphTest2, Blackboard) {
    RenderGraphBlackboard bb{};

    struct MyStuff {
        RenderGraphResourceHandle img0{};
        RenderGraphResourceHandle img1{};
    };

    struct MyStuff1 {};

    auto& s1 = bb.insert<MyStuff>();
    s1.img0 = {123};
    s1.img1 = {111};

    bb.insert<MyStuff1>();
    EXPECT_THAT(bb, SizeIs(2));

    const auto& s2 = bb.get<MyStuff>();
    EXPECT_EQ(s2.img0.id, 123);
    EXPECT_EQ(s2.img1.id, 111);

    // Attempting to add another struct results in termination.
    EXPECT_DEATH_IF_SUPPORTED(bb.insert<MyStuff1>(), "");
}

} // namespace
} // namespace crisp::test